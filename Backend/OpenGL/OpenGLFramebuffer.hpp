#pragma once

#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>

namespace Physara::RHI
{
    class OpenGLFramebuffer final : public RHIFramebuffer
    {
    public:
        explicit OpenGLFramebuffer(const RHIFramebufferDesc &desc);
        ~OpenGLFramebuffer() override;

        std::uint32_t GetWidth() const override { return m_Width; }
        std::uint32_t GetHeight() const override { return m_Height; }

        GLuint GetID() const { return m_ID; }
        std::uint32_t GetColorAttachmentCount() const;

    private:
        GLuint m_ID{0};
        RHIFramebufferDesc m_Desc{};

        std::uint32_t m_Width{0};
        std::uint32_t m_Height{0};
    };
}