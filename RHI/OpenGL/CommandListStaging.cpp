#include "CommandList.hpp"

/// @file
/// @brief 以实际像素或块范围搬运 staging 数据, 发出前取得原物理包保活, 退休时才能发表 Read.

#include <algorithm>
#include <new>

#include "../Common/Transfer.hpp"
#include "Staging.hpp"

namespace Physara::RHI::OpenGL
{
    // Read 的每个目标层分别登记发布位置. Write 只需登记整个 PBO 的借用, 各源层在此单独核对 ready.
    UploadWorkPacket::StagingLease *OpenGLCommandList::PrepareStagingUse(StagingAccess &access, bool &ready,
                                                                        CpuAccessMode mode, const char *operation)
    {
        if (access.mapped || (access.packet && access.packet != m_Packet) ||
            (mode == CpuAccessMode::Write && !ready))
        {
            m_Device->Report(operation, "InvalidState: staging mapped, held by another packet or not written");
            return nullptr;
        }
        auto &leases = m_Packet->staging;
        bool *readReady = mode == CpuAccessMode::Read ? &ready : nullptr;
        const auto found = std::ranges::find_if(leases, [&](const UploadWorkPacket::StagingLease &lease)
        {
            return lease.access == &access && lease.readReady == readReady;
        });
        if (found != leases.end())
        {
            return &*found;
        }
        if (leases.size() == UploadItemLimit)
        {
            m_Device->ReportBudget(operation, 1, leases.size(), UploadItemLimit);
            return nullptr;
        }
        try
        {
            return &leases.emplace_back(&access, readReady);
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report(operation, "CPU allocation failed before staging transfer");
            return nullptr;
        }
    }

    void OpenGLCommandList::RecordStagingUse(UploadWorkPacket::StagingLease &lease) noexcept
    {
        // 从第一条可能发出的命令开始拒绝 CPU 借用, 旧 Read 数据不可冒充本次结果.
        lease.issued = true;
        lease.access->packet = m_Packet;
        if (lease.readReady)
        {
            lease.publish = true;
            lease.graphUse = m_Packet->useMode == UseMode::GraphManaged;
            *lease.readReady = false;
        }
    }

    void OpenGLCommandList::CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                                        const TextureSlice &srcSlice)
    {
        CopyStaging(dest, destSlice, src, srcSlice, CpuAccessMode::Read);
    }

    void OpenGLCommandList::CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                                        const TextureSlice &srcSlice)
    {
        CopyStaging(src, srcSlice, dest, destSlice, CpuAccessMode::Write);
    }

    // Read 将 GPU 源下载到 pack PBO, Write 从 unpack PBO 上传到 GPU 目标. 两端可选择不同的层和矩形起点.
    void OpenGLCommandList::CopyStaging(StagingTexture *staging, const TextureSlice &stagingSlice, Texture *texture,
                                        const TextureSlice &textureSlice, CpuAccessMode mode)
    {
        const bool read = mode == CpuAccessMode::Read;
        const auto *operation = read ? "CopyTexture.ReadStaging" : "CopyTexture.WriteStaging";
        if (!Recording(operation))
        {
            return;
        }
        auto *cpu = dynamic_cast<OpenGLStagingTexture *>(staging);
        if (!cpu || cpu->m_Device.Get() != m_Device.Get() || cpu->m_Mode != mode)
        {
            m_Device->Report(operation, "InvalidArgument: staging implementation, device or CPU direction");
            return;
        }
        auto *gpu = TextureFor(texture, operation);
        if (!gpu)
        {
            return;
        }
        const auto &desc = gpu->GetDesc();
        const auto support = m_Device->GetTransferFormatSupport(desc.format, desc.dimension);
        TextureSlice host, device;
        if (desc.format != cpu->m_Desc.format || !(read ? support.stagingRead : support.stagingWrite) ||
            !ResolvePlanarSlice(cpu->m_Desc, stagingSlice, host) || !ResolvePlanarSlice(desc, textureSlice, device) ||
            host.width != device.width || host.height != device.height)
        {
            m_Device->Report(operation, "Unsupported or InvalidArgument: staging format, sample or copy region");
            return;
        }
        const auto &format = GetFormatInfo(desc.format);
        const auto block = std::size_t(format.blockSize);
        const bool compressed = block > 1;
        if (!Transfer::StagingCopyRegionSupported(format, cpu->m_Desc, host, desc, device))
        {
            m_Device->Report(operation, "Unsupported: compressed staging requires matching full mips");
            return;
        }

        auto &layout = cpu->Footprint(host);
        if (!Retain(cpu, cpu->m_InternalOwners) || !Retain(gpu, gpu->m_InternalOwners))
        {
            return;
        }
        auto *lease = PrepareStagingUse(cpu->m_Access, layout.ready, mode, operation);
        if (!lease || !UseTexture(*gpu, {device.mipLevel, 1, device.arraySlice, 1}, TextureAspect::Color,
                                 read ? ResourceStates::CopySource : ResourceStates::CopyDest, operation,
                                 ResourceStates::Unknown, StorageAccess::None, GL_TEXTURE_UPDATE_BARRIER_BIT,
                                 !read && Transfer::WholeSlice(desc, device)) ||
            !m_Device->BeginNative(operation) || !PrepareNativeAccesses(operation))
        {
            return;
        }

        // PBO 的行距取 staging 所选 mip 的整行宽度. 矩形起点计入字节偏移, GL 仍按完整行距跳到下一行.
        const auto offset = layout.offset + (host.y / block) * layout.rowPitch + (host.x / block) * format.bytesPerBlock;
        const auto rowLength = compressed ? 0 : GLint(Transfer::MipSize(cpu->m_Desc.width, host.mipLevel));
        const auto alignment = compressed ? 1 : 4;
        auto &context = m_Device->m_ContextState;
        if (read)
        {
            context.ApplyPack(cpu->Name(), alignment, rowLength);
        }
        else
        {
            context.ApplyUnpack(cpu->Name(), alignment, rowLength);
        }
        if (!m_Device->CheckNative(operation, cpu->Name()))
        {
            m_Packet->invalid = true;
            return;
        }

        // staging backing 只参与像素传输和独占 CPU 映射, 没有 Shader 写入需要额外同步.
        // 最后一行只计实际 texel 的字节, 其前各行才跨完整 pitch. BC5 的整级传输包含所有尾块.
        const auto size = compressed ? layout.bytes :
            (std::size_t(host.height) - 1) * layout.rowPitch + std::size_t(host.width) * format.bytesPerBlock;
        auto *pointer = reinterpret_cast<void *>(offset);
        const GLint mip = GLint(device.mipLevel), x = GLint(device.x), y = GLint(device.y), z = GLint(device.arraySlice);
        const GLsizei width = GLsizei(device.width), height = GLsizei(device.height);
        const GLuint name = compressed ? gpu->CompressedMipName(device.mipLevel) : gpu->Name();
        GLenum external = 0, type = 0;
        if (!compressed)
        {
            // 此处颜色格式已经由 transfer caps 限为同一套尺寸和外部数据解释.
            if (!GetUploadFormat(desc.format, external, type))
            {
                m_Device->Report(operation, "Unsupported: no external color data mapping");
                return;
            }
        }
        RecordStagingUse(*lease);
        m_Packet->issuedWork = true;
        if (read && compressed)
        {
            // 采用的 RGTC profile 只读二维完整 mip, 直接取该层的压缩块存储.
            glGetCompressedTextureImage(name, 0, GLsizei(size), pointer);
        }
        else if (read)
        {
            glGetTextureSubImage(name, mip, x, y, z, width, height, 1, external, type, GLsizei(size), pointer);
        }
        else if (compressed)
        {
            const auto internal = m_Device->GetFormatCaps(desc.format, desc.dimension).internalFormat;
            glCompressedTextureSubImage2D(name, 0, x, y, width, height, internal, GLsizei(size), pointer);
        }
        else if (desc.dimension == TextureDimension::Texture2D)
        {
            glTextureSubImage2D(name, mip, x, y, width, height, external, type, pointer);
        }
        else
        {
            glTextureSubImage3D(name, mip, x, y, z, width, height, 1, external, type, pointer);
        }
        if (m_Device->CheckNative(operation, name))
        {
            RecordNativeAccesses();
        }
        else
        {
            m_Packet->invalid = true;
        }
    }
}
