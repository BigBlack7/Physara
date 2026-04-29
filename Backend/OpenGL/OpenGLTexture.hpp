#pragma once

#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::RHI
{
    class OpenGLTexture final : public RHITexture
    {
    public:
        explicit OpenGLTexture(const RHITextureDesc &desc);
        ~OpenGLTexture() override;

        std::uint32_t GetWidth() const override;
        std::uint32_t GetHeight() const override;
        std::uint32_t GetMipLevels() const override;
        std::uint32_t GetArrayLayers() const override;
        TextureFormat GetFormat() const override;
        TextureDimension GetDimension() const override;
        TextureUsageFlags GetUsage() const override;

        GLuint GetGLID()const { return m_ID; }
        GLenum GetGLTarget() const { return m_Target; }

        void Upload(std::uint32_t mip, std::uint32_t layer, const void *data, std::uint32_t dataSize);
        void GenerateMipmaps();

    private:
        GLuint m_ID{0};
        GLenum m_Target{GL_TEXTURE_2D};
        RHITextureDesc m_Desc{};
    };
}