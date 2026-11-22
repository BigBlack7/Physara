#include "CommandList.hpp"

/// @file
/// @brief 执行 Buffer 和纹理上传, 为 Draw 和 Dispatch 保存动态常量版本. 普通源字节在本次调用内消费.

#include <cstring>
#include <limits>
#include <new>
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    // 确认容量, 预算和 CopyDest, 图内另核对本次确切字节权限. 小写直接上传, 大写经过包独有的映射区.
    void OpenGLCommandList::WriteBuffer(Buffer *buffer, const void *data, size_t size, uint64_t offset)
    {
        auto *packet = Recording("WriteBuffer");
        if (packet == nullptr)
        {
            return;
        }
        auto *native = BufferFor(buffer, "WriteBuffer");
        if (native == nullptr)
        {
            return;
        }
        if (native->m_Desc.cpuAccess != CpuAccessMode::None)
        {
            m_Device->Report("WriteBuffer", "InvalidArgument: fill Write staging by MapBuffer and UnmapBuffer");
            return;
        }
        if (offset > native->m_Desc.byteSize || size > native->m_Desc.byteSize - offset ||
            offset > std::uint64_t(std::numeric_limits<GLintptr>::max()) ||
            size > std::size_t(std::numeric_limits<GLsizeiptr>::max()) || (size != 0 && data == nullptr))
        {
            m_Device->Report("WriteBuffer", "InvalidArgument: source or destination interval");
            return;
        }
        const bool isVolatile = native->m_Desc.isVolatile;
        if (isVolatile && (offset != 0 || size == 0))
        {
            m_Device->Report("WriteBuffer", "InvalidArgument: volatile write requires a nonempty prefix");
            return;
        }
        if (size == 0)
        {
            // 严格空区间不取得上传容量/lease, 也不解引用 data; offset 已按实际容量检查.
            return;
        }
        if (packet->writes == UploadItemLimit)
        {
            m_Device->ReportBudget("WriteBuffer.WriteCount", 1, packet->writes, UploadItemLimit);
            return;
        }
        if (size > UploadCapacityLimit - packet->logicalBytes)
        {
            m_Device->ReportBudget("WriteBuffer", size, packet->logicalBytes, UploadCapacityLimit);
            return;
        }
        if (isVolatile)
        {
            WriteVolatile(*native, data, size);
            return;
        }
        if (!Retain(native, native->m_InternalOwners) ||
            !UseBuffer(*native, ResourceStates::CopyDest, "WriteBuffer", BufferRange{offset, size}, false,
                       StorageAccess::None, GL_BUFFER_UPDATE_BARRIER_BIT, true))
        {
            return;
        }
        UploadSlice source;
        // 小写直接由 DSA 消费源字节; 大写先复制到本包映射区段, 两条路径都不跨返回保存源指针.
        if (size > SmallBufferLimit)
        {
            if (!m_Device->AllocateUpload(*packet, size, source))
            {
                return;
            }
            // coherent 只解决 CPU 可见性, 本块直到 fence Complete 都不复用/覆写.
            std::memcpy(source.cpu, data, size);
        }
        if (!m_Device->BeginNative("WriteBuffer", native->m_Desc.debugName.c_str()))
        {
            packet->invalid = true;
            return;
        }
        if (!PrepareNativeAccesses("WriteBuffer"))
        {
            return;
        }
        // 从这里起原生调用可能已经消费目标或映射源. 先登记 issuedWork, 即使调用带错也必须保护整个前缀.
        ++packet->writes;
        packet->logicalBytes += size;
        packet->issuedWork = true;
        ++m_Device->m_CommandCounts.uploads;
        m_Device->m_CommandCounts.uploadBytes += size;
        if (size <= SmallBufferLimit)
        {
            glNamedBufferSubData(native->m_Deletion->name, GLintptr(offset), GLsizeiptr(size), data);
        }
        else
        {
            glCopyNamedBufferSubData(source.buffer, native->m_Deletion->name, GLintptr(source.offset), GLintptr(offset),
                                     GLsizeiptr(size));
        }
        if (m_Device->CheckNative("WriteBuffer", native->m_Deletion->name, native->m_Desc.debugName.c_str()))
        {
            RecordNativeAccesses();
        }
        else
        {
            packet->invalid = true;
        }
    }

    void OpenGLCommandList::WriteVolatile(OpenGLBuffer &buffer, const void *data, std::size_t size)
    {
        if (!CheckGraphBufferUse(buffer, ResourceStates::CopyDest, "WriteBuffer.Volatile", BufferRange{0, size}))
        {
            return;
        }
        auto &packet = *m_Packet;
        const auto count = buffer.m_Versions.size();
        // 从上次写入的后一槽开始绕回. 非零身份表示仍由某个包持有, 包退休统一清零.
        auto version = static_cast<std::size_t>(buffer.m_NextVersion);
        std::size_t checked = 0;
        while (checked < count && buffer.m_Versions[version] != 0)
        {
            version = (version + 1) % count;
            ++checked;
        }
        if (checked == count)
        {
            m_Device->ReportBudget("WriteBuffer.VolatileVersions", 1, count, count);
            return;
        }
        if (!Retain(&buffer, buffer.m_InternalOwners))
        {
            return;
        }
        try
        {
            packet.volatileLeases.push_back({&buffer, static_cast<std::uint32_t>(version)});
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report("WriteBuffer.Volatile", "CPU allocation failed before reserving a version");
            return;
        }
        // 最后才发布新版本. 早先 Draw 仍访问原区段, 此处的 CPU 复制不发出设备工作.
        std::memcpy(buffer.m_Mapping + version * buffer.m_VersionStride, data, size);
        buffer.m_Versions[version] = packet.recordingId;
        buffer.m_LatestRecording = packet.recordingId;
        buffer.m_LatestVersion = static_cast<std::uint32_t>(version);
        buffer.m_NextVersion = static_cast<std::uint32_t>((version + 1) % count);
        buffer.m_ValidPrefix = size;
        ++packet.writes;
        packet.logicalBytes += size;
    }

    void OpenGLCommandList::SetPushConstants(const void *data, size_t byteSize)
    {
        auto *packet = Recording("SetPushConstants");
        if (!packet)
        {
            return;
        }
        if (!packet->graphicsSet && !packet->computeSet)
        {
            m_Device->Report("SetPushConstants", "InvalidState: set an executable state before push constants");
            return;
        }
        const auto &plan = packet->computeSet ?
            static_cast<const OpenGLComputePipeline *>(packet->compute.pipeline)->Plan() :
            static_cast<const OpenGLGraphicsPipeline *>(packet->graphics.pipeline)->Plan();
        if (!data || byteSize == 0 || byteSize != plan.pushBytes)
        {
            m_Device->Report("SetPushConstants", "InvalidArgument: push bytes must match the active pipeline layout");
            return;
        }
        if (packet->writes == UploadItemLimit || byteSize > UploadCapacityLimit - packet->logicalBytes)
        {
            m_Device->Report("SetPushConstants", "CapacityExceeded: recording write count or byte budget");
            return;
        }
        UploadSlice slice;
        if (!m_Device->AllocateUpload(*packet, plan.pushBackingBytes, slice,
                                       std::size_t(m_Device->m_Caps.shader.uniformAlignment)))
        {
            return;
        }
        // 只读调用者声明的逻辑字节. 驱动链接后的块末尾可能有对齐填充, 填充由后端写零.
        std::memcpy(slice.cpu, data, byteSize);
        std::memset(slice.cpu + byteSize, 0, plan.pushBackingBytes - byteSize);
        packet->push = slice;
        ++packet->writes;
        packet->logicalBytes += byteSize;
    }

    // 先确定合法 footprint 和紧凑 PBO 容量, 逐行只复制 texel, 最后按实际 target 发出上传, 不翻转或转色.
    void OpenGLCommandList::WriteTexture(Texture *texture, uint32_t arraySlice, uint32_t mipLevel, const void *data,
                                         size_t rowPitch, size_t depthPitch)
    {
        auto *packet = Recording("WriteTexture");
        if (packet == nullptr)
        {
            return;
        }
        auto *native = TextureFor(texture, "WriteTexture");
        if (native == nullptr)
        {
            return;
        }
        const auto &desc = native->m_Desc;
        // 先排除内容 profile 再解释源布局; 已支持分配的 depth/stencil/MS 不因此取得上传能力.
        GLenum format = 0;
        GLenum type = 0;
        if (!GetUploadFormat(desc.format, format, type) || desc.dimension == TextureDimension::Texture2DMS ||
            desc.dimension == TextureDimension::Texture2DMSArray)
        {
            m_Device->Report("WriteTexture", "Unsupported: color non-MS transfer profile required");
            return;
        }
        TextureUploadLayout layout;
        if (data == nullptr || !BuildUploadLayout(desc, arraySlice, mipLevel, rowPitch, depthPitch, layout))
        {
            m_Device->Report("WriteTexture", "InvalidArgument: source, subresource or pitch footprint");
            return;
        }
        if (packet->writes == UploadItemLimit)
        {
            m_Device->ReportBudget("WriteTexture.WriteCount", 1, packet->writes, UploadItemLimit);
            return;
        }
        if (layout.tightBytes > UploadCapacityLimit - packet->logicalBytes)
        {
            m_Device->ReportBudget("WriteTexture", layout.tightBytes, packet->logicalBytes, UploadCapacityLimit);
            return;
        }
        TextureSubresourceSet subresources;
        subresources.baseMipLevel = mipLevel;
        subresources.numMipLevels = 1;
        subresources.baseArraySlice = arraySlice;
        subresources.numArraySlices = 1;
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, subresources, TextureAspect::Color, ResourceStates::CopyDest, "WriteTexture",
                        ResourceStates::Unknown, StorageAccess::None, GL_TEXTURE_UPDATE_BARRIER_BIT, true))
        {
            return;
        }
        UploadSlice source;
        if (!m_Device->AllocateUpload(*packet, layout.tightBytes, source))
        {
            return;
        }
        // 借用指针须覆盖已验证 footprint; 每行只读取 texel, 允许 padding 非整 texel 倍数.
        const auto *bytes = static_cast<const std::byte *>(data);
        for (std::size_t z = 0; z < layout.depth; ++z)
        {
            for (std::size_t y = 0; y < layout.height; ++y)
            {
                std::memcpy(source.cpu + (z * layout.height + y) * layout.rowBytes,
                            bytes + z * layout.sliceStride + y * layout.rowStride, layout.rowBytes);
            }
        }
        if (!m_Device->BeginNative("WriteTexture", desc.debugName.c_str()))
        {
            packet->invalid = true;
            return;
        }
        // 普通上传已经逐行紧排. 前一次 staging 上传可能留下较宽的行长与四字节对齐.
        m_Device->m_ContextState.ApplyUnpack(source.buffer);
        if (!m_Device->CheckNative("WriteTexture.Unpack", source.buffer))
        {
            packet->invalid = true;
            return;
        }
        if (!PrepareNativeAccesses("WriteTexture"))
        {
            return;
        }
        // 从此指针表示已绑定 PBO 内的字节 offset, 而不是 CPU 地址. 首个可能发出的写前保留全部包所有权.
        const auto *offset = reinterpret_cast<const void *>(source.offset);
        ++packet->writes;
        packet->logicalBytes += layout.tightBytes;
        packet->issuedWork = true;
        // 一次完整子资源写算一笔. 行尾填充与 PBO 对齐间隙都在拷入紧密区段时去掉.
        ++m_Device->m_CommandCounts.uploads;
        m_Device->m_CommandCounts.uploadBytes += layout.tightBytes;
        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
            glTextureSubImage1D(native->Name(), GLint(mipLevel), 0, GLsizei(layout.width), format, type, offset);
            break;
        case TextureDimension::Texture1DArray:
            glTextureSubImage2D(native->Name(), GLint(mipLevel), 0, GLint(arraySlice), GLsizei(layout.width), 1, format,
                                type, offset);
            break;
        case TextureDimension::Texture2D:
            glTextureSubImage2D(native->Name(), GLint(mipLevel), 0, 0, GLsizei(layout.width), GLsizei(layout.height),
                                format, type, offset);
            break;
        default:
            glTextureSubImage3D(native->Name(), GLint(mipLevel), 0, 0, GLint(arraySlice), GLsizei(layout.width),
                                GLsizei(layout.height), GLsizei(layout.depth), format, type, offset);
            break;
        }
        if (m_Device->CheckNative("WriteTexture", native->Name(), desc.debugName.c_str()))
        {
            RecordNativeAccesses();
        }
        else
        {
            packet->invalid = true;
        }
    }
}
