#include "Staging.hpp"

/// @file
/// @brief 计算 staging 布局, 分配有界 backing, 只在真实已退休且没有冲突借用时调用 native map.

#include <array>
#include <new>

#include "../Common/Transfer.hpp"
#include "Upload.hpp"

namespace Physara::RHI::OpenGL
{
    OpenGLStagingTexture::OpenGLStagingTexture(OpenGLDevice &device, const TextureDesc &desc, CpuAccessMode mode)
        : m_Device(&device), m_Desc(desc), m_Mode(mode), m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Buffer))
    {
    }

    OpenGLStagingTexture::~OpenGLStagingTexture()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    bool ResolvePlanarSlice(const TextureDesc &desc, const TextureSlice &input, TextureSlice &resolved) noexcept
    {
        if (!Transfer::Planar(desc.dimension) || desc.sampleCount != 1 ||
            input.mipLevel >= desc.mipLevels || input.arraySlice >= desc.arraySize)
        {
            return false;
        }
        resolved = input.Resolve(desc);
        const auto width = Transfer::MipSize(desc.width, resolved.mipLevel);
        const auto height = Transfer::MipSize(desc.height, resolved.mipLevel);
        return resolved.z == 0 && resolved.depth == 1 && resolved.width != 0 && resolved.height != 0 &&
               resolved.x <= width && resolved.y <= height &&
               resolved.width <= width - resolved.x && resolved.height <= height - resolved.y;
    }

    // 设备在提交/删除一侧维护额度. 创建失败的未发布名字沿通常回滚, 仅成功分配计费.
    bool OpenGLDevice::AllocateStaging(NativeDeletion &record, std::size_t bytes, CpuAccessMode mode,
                                        const char *operation, const char *label)
    {
        if (bytes > StagingCapacityLimit - m_StagingCapacity)
        {
            ReportBudget(operation, bytes, m_StagingCapacity, StagingCapacityLimit);
            return false;
        }
        if (m_StagingObjects == StagingObjectLimit)
        {
            ReportBudget(operation, 1, m_StagingObjects, StagingObjectLimit);
            return false;
        }
        if (!BeginNative(operation, label))
        {
            return false;
        }
        glCreateBuffers(1, &record.name);
        if (!CheckNative(operation, record.name, label))
        {
            return false;
        }
        if (record.name == 0)
        {
            Report(operation, "NativeFailure: zero staging buffer name", label);
            return false;
        }
        glNamedBufferStorage(record.name, GLsizeiptr(bytes), nullptr,
                             mode == CpuAccessMode::Read ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT);
        if (!CheckNative(operation, record.name, label))
        {
            return false;
        }
        record.stagingBytes = bytes;
        m_StagingCapacity += bytes;
        ++m_StagingObjects;
        return true;
    }

    StagingTextureHandle OpenGLDevice::CreateStagingTexture(const TextureDesc &desc, CpuAccessMode mode)
    {
        constexpr auto Operation = "CreateStagingTexture";
        if (!RequireContext(Operation))
        {
            return nullptr;
        }
        const auto support = GetTransferFormatSupport(desc.format, desc.dimension);
        const auto &format = GetFormatInfo(desc.format);
        if (!Transfer::StagingTextureShape(desc) || !ValidTextureShape(desc) ||
            !Transfer::CompatibleTextureUsage(desc, format) ||
            !ValidInitialState(desc.initialState, desc.keepInitialState) ||
            !((mode == CpuAccessMode::Read && support.stagingRead) ||
              (mode == CpuAccessMode::Write && support.stagingWrite)))
        {
            Report(Operation, "Unsupported or InvalidArgument: staging format, shape or CPU direction",
                   desc.debugName.c_str());
            return nullptr;
        }

        // 同一 mip 的各层共用尺寸和行距. 先按像素块向上取整, 再按四字节对齐, 从剩余额度推导可容纳的行数.
        const std::size_t block = format.blockSize;
        std::array<StagingFootprint, 32> layouts{};
        std::size_t total = 0;
        for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
        {
            const auto blocks = (std::size_t(Transfer::MipSize(desc.width, mip)) + block - 1) / block;
            const auto rows = (std::size_t(Transfer::MipSize(desc.height, mip)) + block - 1) / block;
            auto &layout = layouts[mip];
            layout.offset = total;
            if (!CheckedAlign(blocks * format.bytesPerBlock, 4, layout.rowPitch) ||
                layout.rowPitch > (StagingCapacityLimit - total) / rows / desc.arraySize)
            {
                Report(Operation, "CapacityExceeded: staging footprint", desc.debugName.c_str());
                return nullptr;
            }
            layout.bytes = layout.rowPitch * rows;
            total += layout.bytes * desc.arraySize;
        }
        try
        {
            auto texture = std::make_unique<OpenGLStagingTexture>(*this, desc, mode);
            // 将每级的首层偏移展开为稳定表项. 录制包随后会借用表项的 ready 标志直至退休.
            texture->m_Footprints.resize(std::size_t(desc.mipLevels) * desc.arraySize);
            for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
            {
                auto layout = layouts[mip];
                for (std::uint32_t layer = 0; layer < desc.arraySize; ++layer)
                {
                    texture->m_Footprints[std::size_t(mip) * desc.arraySize + layer] = layout;
                    layout.offset += layout.bytes;
                }
            }
            if (!AllocateStaging(*texture->m_Deletion, total, mode, Operation, desc.debugName.c_str()))
            {
                return nullptr;
            }
            texture->m_Published = true;
            return StagingTextureHandle::Create(texture.release());
        }
        catch (const std::bad_alloc &)
        {
            Report(Operation, "CPU allocation failed before staging publication", desc.debugName.c_str());
            return nullptr;
        }
    }

    // GPU 占用由原包退休解除. 即使驱动可能已经执行命令, Map 也不替调用者等待或查询提交.
    void *OpenGLDevice::MapStaging(StagingAccess &access, bool &ready, GLuint name, std::size_t offset,
                                  std::size_t size, CpuAccessMode mode, const char *operation)
    {
        if (access.packet != nullptr)
        {
            return nullptr;
        }
        if (access.mapped != nullptr || (mode == CpuAccessMode::Read && !ready))
        {
            Report(operation, "InvalidState: already mapped or no completed staging data");
            return nullptr;
        }
        if (!BeginNative(operation))
        {
            return nullptr;
        }
        auto *result = glMapNamedBufferRange(name, GLintptr(offset), GLsizeiptr(size),
                                             mode == CpuAccessMode::Read ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT);
        if (!CheckNative(operation, name))
        {
            return nullptr;
        }
        if (!result)
        {
            Report(operation, "NativeFailure: staging map returned null");
            return nullptr;
        }
        access.mapped = &ready;
        return result;
    }

    // 结束借用时分别读取数据完整性和 GL 错误. 驱动可以在没有 GL 错误时用 GL_FALSE 报告存储损坏.
    OpenGLDevice::StagingUnmapResult OpenGLDevice::UnmapStaging(StagingAccess &access, GLuint name,
                                                                CpuAccessMode mode, const char *operation)
    {
        if (access.mapped == nullptr)
        {
            Report(operation, "InvalidState: staging is not mapped");
            return StagingUnmapResult::Rejected;
        }
        if (!BeginNative(operation))
        {
            return StagingUnmapResult::Rejected;
        }
        const bool dataValid = glUnmapNamedBuffer(name) == GL_TRUE;
        const bool nativeValid = CheckNative(operation, name);
        const bool ok = dataValid && nativeValid;
        *access.mapped = ok && (mode == CpuAccessMode::Write || *access.mapped);
        access.mapped = nullptr;
        if (nativeValid && !dataValid)
        {
            Report(operation, "NativeFailure: unmapped staging data invalidated");
            return StagingUnmapResult::StoreLost;
        }
        return nativeValid ? StagingUnmapResult::Unmapped : StagingUnmapResult::Rejected;
    }

    void *OpenGLDevice::MapBuffer(Buffer *buffer, CpuAccessMode mode)
    {
        constexpr auto Operation = "MapBuffer";
        if (!RequireContext(Operation))
        {
            return nullptr;
        }
        auto *native = dynamic_cast<OpenGLBuffer *>(buffer);
        if (!native || native->Owner() != this || mode == CpuAccessMode::None || native->m_Desc.cpuAccess != mode)
        {
            Report(Operation, "InvalidArgument: buffer implementation, owning device or CPU direction");
            return nullptr;
        }
        return MapStaging(native->m_Staging, native->m_StagingReady, native->Name(), 0,
                          std::size_t(native->m_Desc.byteSize), mode, Operation);
    }

    void OpenGLDevice::UnmapBuffer(Buffer *buffer)
    {
        constexpr auto Operation = "UnmapBuffer";
        if (!RequireContext(Operation))
        {
            return;
        }
        auto *native = dynamic_cast<OpenGLBuffer *>(buffer);
        if (!native || native->Owner() != this || native->m_Desc.cpuAccess == CpuAccessMode::None)
        {
            Report(Operation, "InvalidArgument: not an owned staging buffer");
            return;
        }
        // Buffer 的唯一就绪标记已经由 UnmapStaging 更新.
        static_cast<void>(UnmapStaging(native->m_Staging, native->Name(), native->m_Desc.cpuAccess, Operation));
    }

    void *OpenGLDevice::MapStagingTexture(StagingTexture *texture, const TextureSlice &slice, CpuAccessMode mode,
                                         size_t *outRowPitch)
    {
        constexpr auto Operation = "MapStagingTexture";
        if (!RequireContext(Operation))
        {
            return nullptr;
        }
        auto *native = dynamic_cast<OpenGLStagingTexture *>(texture);
        TextureSlice selected;
        if (!native || native->m_Device.Get() != this || mode != native->m_Mode || !outRowPitch ||
            !ResolvePlanarSlice(native->m_Desc, slice, selected) || !Transfer::WholeSlice(native->m_Desc, selected))
        {
            Report(Operation, "InvalidArgument: owned staging texture, CPU direction, full slice and pitch required");
            return nullptr;
        }
        auto &layout = native->Footprint(selected);
        auto *result = MapStaging(native->m_Access, layout.ready, native->Name(), layout.offset, layout.bytes,
                                  mode, Operation);
        if (result)
        {
            *outRowPitch = layout.rowPitch;
        }
        return result;
    }

    void OpenGLDevice::UnmapStagingTexture(StagingTexture *texture)
    {
        constexpr auto Operation = "UnmapStagingTexture";
        if (!RequireContext(Operation))
        {
            return;
        }
        auto *native = dynamic_cast<OpenGLStagingTexture *>(texture);
        if (!native || native->m_Device.Get() != this)
        {
            Report(Operation, "InvalidArgument: not an owned staging texture");
            return;
        }
        if (UnmapStaging(native->m_Access, native->Name(), native->m_Mode, Operation) == StagingUnmapResult::StoreLost)
        {
            // GL 以整个 Buffer 报损. 同一 PBO 的其他层和 mip 也须重新写入或接收已完成的复制.
            for (auto &footprint : native->m_Footprints)
            {
                footprint.ready = false;
            }
        }
    }
}
