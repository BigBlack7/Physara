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

        glCreateTextures(m_Target, 1, &m_ID);

        const auto fmt = ToGLTextureFormat(m_Desc.format);

        if (m_Target == GL_TEXTURE_2D || m_Target == GL_TEXTURE_CUBE_MAP)
        {
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
        assert(data != nullptr);
        assert(mip < m_Desc.mipLevels);

        const auto fmt = ToGLTextureFormat(m_Desc.format);
        const bool compressed = IsCompressedTextureFormat(m_Desc.format);

        const std::uint32_t w = std::max(1u, m_Desc.width >> mip);
        const std::uint32_t h = std::max(1u, m_Desc.height >> mip);

        if (m_Target == GL_TEXTURE_2D)
        {
            assert(layer == 0);

            if (compressed)
            {
                assert(dataSize > 0);
                glCompressedTextureSubImage2D(
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
                glTextureSubImage2D(
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
        glGenerateTextureMipmap(m_ID);
    }
}