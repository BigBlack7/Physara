#include "OpenGLFramebuffer.hpp"

#include <cassert>
#include <string>
#include <vector>

#include <Engine/Core/Log.hpp>

#include <Backend/OpenGL/OpenGLTexture.hpp>

namespace Physara::RHI
{
    namespace Internal
    {
        static const char *FramebufferStatusToString(GLenum status)
        {
            switch (status)
            {
            case GL_FRAMEBUFFER_COMPLETE:
                return "GL_FRAMEBUFFER_COMPLETE";
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            case GL_FRAMEBUFFER_UNSUPPORTED:
                return "GL_FRAMEBUFFER_UNSUPPORTED";
            default:
                return "GL_FRAMEBUFFER_UNKNOWN";
            }
        }
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const RHIFramebufferDesc &desc)
        : m_Desc(desc), m_Width(desc.width), m_Height(desc.height)
    {
        glCreateFramebuffers(1, &m_ID);

        // 绑定颜色附件
        const std::uint32_t colorCount = static_cast<std::uint32_t>(m_Desc.colorAttachments.size());
        for (std::uint32_t i = 0; i < colorCount; ++i)
        {
            auto *tex = m_Desc.colorAttachments[i];
            if (!tex)
            {
                continue;
            }

            auto *glTex = static_cast<OpenGLTexture *>(tex);
            glNamedFramebufferTexture(
                m_ID,
                static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                glTex->GetGLID(),
                static_cast<GLint>(m_Desc.mipLevel));
        }

        // 绑定深度附件
        if (m_Desc.depthAttachment)
        {
            auto *glDepth = static_cast<OpenGLTexture *>(m_Desc.depthAttachment);
            glNamedFramebufferTexture(
                m_ID,
                GL_DEPTH_ATTACHMENT,
                glDepth->GetGLID(),
                static_cast<GLint>(m_Desc.mipLevel));
        }

        // Draw buffers
        if (colorCount > 0)
        {
            std::vector<GLenum> drawBuffers;
            drawBuffers.reserve(colorCount);
            for (std::uint32_t i = 0; i < colorCount; ++i)
            {
                drawBuffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
            }
            glNamedFramebufferDrawBuffers(m_ID, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        }
        else
        {
            glNamedFramebufferDrawBuffers(m_ID, 0, nullptr);
        }

        const GLenum status = glCheckNamedFramebufferStatus(m_ID, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            PHYSARA_CORE_ERROR("FBO incomplete: {}", Internal::FramebufferStatusToString(status));
        }
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        if (m_ID != 0)
        {
            glDeleteFramebuffers(1, &m_ID);
            m_ID = 0;
        }
    }

    std::uint32_t OpenGLFramebuffer::GetColorAttachmentCount() const
    {
        return static_cast<std::uint32_t>(m_Desc.colorAttachments.size());
    }
}