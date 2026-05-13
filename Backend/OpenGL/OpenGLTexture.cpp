#include "OpenGLTexture.hpp"

#include <algorithm>
#include <cassert>

#include <Backend/OpenGL/OpenGLTypeMapping.hpp>

namespace Physara::RHI
{
    OpenGLTexture::OpenGLTexture(const RHITextureDesc &desc) : m_Desc(desc)
    {
        assert(m_Desc.width > 0);
        assert(m_Desc.height > 0);
        assert(m_Desc.mipLevels > 0);
        assert(m_Desc.samples == 1 && "MSAA textures are not supported yet."); // MSAA纹理暂不支持

        // RHI texture dimension -> OpenGL texture target. Cube map在 OpenGL下仍然是2D storage,
        // 但上传单面时使用3D-like subimage(layer = face index)的DSA接口
        if (m_Desc.dimension == TextureDimension::Tex2D)
        {
            m_Target = GL_TEXTURE_2D;
        }
        else if (m_Desc.dimension == TextureDimension::TexCube)
        {
            m_Target = GL_TEXTURE_CUBE_MAP;
            assert(m_Desc.arrayLayers == 6 && "Cubemap must have 6 layers.");
        }
        else if (m_Desc.dimension == TextureDimension::Tex2DArray)
        {
            m_Target = GL_TEXTURE_2D_ARRAY;
            assert(m_Desc.arrayLayers > 0);
        }
        else
        {
            assert(false && "Unsupported texture dimension.");
            m_Target = GL_TEXTURE_2D;
        }

        // DSA创建texture object, 不需要 glBindTexture
        glCreateTextures(m_Target, 1, &m_ID);
        const auto fmt = ToGLTextureFormat(m_Desc.format);

        if (m_Target == GL_TEXTURE_2D || m_Target == GL_TEXTURE_CUBE_MAP)
        {
            // 不可变存储: 一次声明所有mip级别, 内存布局固定, 驱动优化更好
            glTextureStorage2D(
                m_ID,
                static_cast<GLsizei>(m_Desc.mipLevels),
                fmt.internalFormat,
                static_cast<GLsizei>(m_Desc.width),
                static_cast<GLsizei>(m_Desc.height));
        }
        else if (m_Target == GL_TEXTURE_2D_ARRAY)
        {
            glTextureStorage3D(
                m_ID,
                static_cast<GLsizei>(m_Desc.mipLevels),
                fmt.internalFormat,
                static_cast<GLsizei>(m_Desc.width),
                static_cast<GLsizei>(m_Desc.height),
                static_cast<GLsizei>(m_Desc.arrayLayers));
        }

        glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, m_Desc.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_ID, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(m_ID, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(m_Desc.mipLevels - 1));

        if (m_Desc.initialData != nullptr)
        {
            // RHITextureDesc目前没有initialDataSize, 因此压缩纹理不能安全走构造期上传
            assert(!IsCompressedTextureFormat(m_Desc.format) &&
                   "Compressed initialData requires an explicit byte size; call Upload() instead.");
            Upload(0, 0, m_Desc.initialData, 0);
        }
    }

    OpenGLTexture::~OpenGLTexture()
    {
        if (m_ID != 0)
        {
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
        }
    }

    std::uint32_t OpenGLTexture::GetWidth() const
    {
        return m_Desc.width;
    }

    std::uint32_t OpenGLTexture::GetHeight() const
    {
        return m_Desc.height;
    }

    std::uint32_t OpenGLTexture::GetMipLevels() const
    {
        return m_Desc.mipLevels;
    }

    std::uint32_t OpenGLTexture::GetArrayLayers() const
    {
        return m_Desc.arrayLayers;
    }

    RHI::TextureFormat OpenGLTexture::GetFormat() const
    {
        return m_Desc.format;
    }

    RHI::TextureDimension OpenGLTexture::GetDimension() const
    {
        return m_Desc.dimension;
    }

    RHI::TextureUsageFlags OpenGLTexture::GetUsage() const
    {
        return m_Desc.usage;
    }

    void OpenGLTexture::Upload(std::uint32_t mip, std::uint32_t layer, const void *data, std::uint32_t dataSize)
    {
        // 上传一个完整subresource: mip指定mip level, layer指cube face/array slice
        // BC6H/BC7走glCompressedTextureSubImage*, 普通格式走glTextureSubImage*
        assert(data != nullptr);
        assert(mip < m_Desc.mipLevels);

        const auto fmt = ToGLTextureFormat(m_Desc.format);
        const bool compressed = IsCompressedTextureFormat(m_Desc.format);

        // 根据mipmap层级计算对应的宽度和高度
        const std::uint32_t w = std::max(1u, m_Desc.width >> mip);
        const std::uint32_t h = std::max(1u, m_Desc.height >> mip);

        if (m_Target == GL_TEXTURE_2D)
        {
            assert(layer == 0);

            if (compressed)
            {
                assert(dataSize > 0);
                glCompressedTextureSubImage2D( // 上传压缩纹理数据到2D纹理
                    m_ID,
                    static_cast<GLint>(mip),
                    0, 0,
                    static_cast<GLsizei>(w),
                    static_cast<GLsizei>(h),
                    fmt.internalFormat,
                    static_cast<GLsizei>(dataSize),
                    data);
            }
            else
            {
                glTextureSubImage2D( // 更新2D纹理的指定区域
                    m_ID,
                    static_cast<GLint>(mip),
                    0, 0,
                    static_cast<GLsizei>(w),
                    static_cast<GLsizei>(h),
                    fmt.baseFormat,
                    fmt.type,
                    data);
            }
        }
        else if (m_Target == GL_TEXTURE_CUBE_MAP || m_Target == GL_TEXTURE_2D_ARRAY)
        {
            const std::uint32_t maxLayer = (m_Target == GL_TEXTURE_CUBE_MAP) ? 6u : m_Desc.arrayLayers;
            assert(layer < maxLayer);

            if (compressed)
            {
                assert(dataSize > 0);
                glCompressedTextureSubImage3D(
                    m_ID,
                    static_cast<GLint>(mip),
                    0, 0, static_cast<GLint>(layer),
                    static_cast<GLsizei>(w),
                    static_cast<GLsizei>(h),
                    1, // depth=1
                    fmt.internalFormat,
                    static_cast<GLsizei>(dataSize),
                    data);
            }
            else
            {
                glTextureSubImage3D(
                    m_ID,
                    static_cast<GLint>(mip),
                    0, 0, static_cast<GLint>(layer),
                    static_cast<GLsizei>(w),
                    static_cast<GLsizei>(h),
                    1, // depth=1
                    fmt.baseFormat,
                    fmt.type,
                    data);
            }
        }
    }

    void OpenGLTexture::GenerateMipmaps()
    {
        // DSA mipmap generation要求texture storage已经包含完整mip chain或至少可生成目标levels
        glGenerateTextureMipmap(m_ID);
    }
}