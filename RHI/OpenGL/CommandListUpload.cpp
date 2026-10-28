#include "CommandList.hpp"

#include <cstring>
#include <limits>

namespace Physara::RHI::OpenGL
{
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
        if (offset > native->m_Desc.byteSize || size > native->m_Desc.byteSize - offset ||
            offset > std::uint64_t(std::numeric_limits<GLintptr>::max()) ||
            size > std::size_t(std::numeric_limits<GLsizeiptr>::max()) || (size != 0 && data == nullptr))
        {
            m_Device->Report("WriteBuffer", "InvalidArgument: source or destination interval");
            return;
        }
        if (size == 0)
        {
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
        if (!Retain(native, native->m_InternalUploadHolds) || !TransferState(*native))
        {
            return;
        }
        UploadSlice source;
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
        ++packet->writes;
        packet->logicalBytes += size;
        packet->issuedWork = true;
        if (size <= SmallBufferLimit)
        {
            glNamedBufferSubData(native->m_Deletion->name, GLintptr(offset), GLsizeiptr(size), data);
        }
        else
        {
            glCopyNamedBufferSubData(source.buffer, native->m_Deletion->name, GLintptr(source.offset), GLintptr(offset),
                                     GLsizeiptr(size));
        }
        if (!m_Device->CheckNative("WriteBuffer", native->m_Deletion->name, native->m_Desc.debugName.c_str()))
        {
            packet->invalid = true;
        }
    }

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
        if (!Retain(native, native->m_InternalUploadHolds) || !TransferState(*native, subresources))
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
        m_Device->m_ContextState.ApplyUnpack(source.buffer);
        if (!m_Device->CheckNative("WriteTexture.Unpack", source.buffer))
        {
            packet->invalid = true;
            return;
        }
        const auto *offset = reinterpret_cast<const void *>(source.offset);
        ++packet->writes;
        packet->logicalBytes += layout.tightBytes;
        packet->issuedWork = true;
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
        if (!m_Device->CheckNative("WriteTexture", native->Name(), desc.debugName.c_str()))
        {
            packet->invalid = true;
        }
    }
}
