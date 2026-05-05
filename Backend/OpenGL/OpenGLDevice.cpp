#include "OpenGLDevice.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <Engine/Core/Log.hpp>

#include <Backend/OpenGL/OpenGLBuffer.hpp>
#include <Backend/OpenGL/OpenGLCommandList.hpp>
#include <Backend/OpenGL/OpenGLFramebuffer.hpp>
#include <Backend/OpenGL/OpenGLPipeline.hpp>
#include <Backend/OpenGL/OpenGLSampler.hpp>
#include <Backend/OpenGL/OpenGLShader.hpp>
#include <Backend/OpenGL/OpenGLTexture.hpp>

namespace Physara::RHI
{
    namespace Internal
    {
        static void APIENTRY GLDebugCallback(
            GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei,
            const GLchar *message,
            const void *)
        {
            if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
            {
                return;
            }

            if (type == GL_DEBUG_TYPE_ERROR)
            {
                PHYSARA_CORE_ERROR("GL Error [{}]: {}", id, message);
            }
            else
            {
                PHYSARA_CORE_WARN("GL Warning [{}]: {}", id, message);
            }
        }
    }

    bool OpenGLDevice::Init(void *windowHandle)
    {
        auto *window = static_cast<GLFWwindow *>(windowHandle);
        if (!window)
        {
            PHYSARA_CORE_ERROR("OpenGLDevice::Init called with null window handle.");
            return false;
        }

        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            PHYSARA_CORE_FATAL("gladLoadGLLoader failed.");
            return false;
        }

        PHYSARA_CORE_INFO("OpenGL {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        PHYSARA_CORE_INFO("GPU: {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

        GLint maxAniso = 1;
#if defined(GL_MAX_TEXTURE_MAX_ANISOTROPY)
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
#elif defined(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
#endif
        if (maxAniso < 1)
        {
            maxAniso = 1;
        }
        m_MaxAnisotropy = maxAniso;

        m_CommandList = std::make_unique<OpenGLCommandList>();

#if defined(PHYSARA_DEBUG)
        if (GLAD_GL_KHR_debug)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(Internal::GLDebugCallback, nullptr);
        }
#endif

        return true;
    }

    void OpenGLDevice::Shutdown()
    {
        m_CommandList.reset();
    }

    std::unique_ptr<RHIBuffer> OpenGLDevice::CreateBuffer(const RHIBufferDesc &desc)
    {
        return std::make_unique<OpenGLBuffer>(desc);
    }

    std::unique_ptr<RHITexture> OpenGLDevice::CreateTexture(const RHITextureDesc &desc)
    {
        return std::make_unique<OpenGLTexture>(desc);
    }

    std::unique_ptr<RHISampler> OpenGLDevice::CreateSampler(const RHISamplerDesc &desc)
    {
        return std::make_unique<OpenGLSampler>(desc);
    }

    std::unique_ptr<RHIShader> OpenGLDevice::CreateShader(ShaderStage stage, const std::string &source)
    {
        return std::make_unique<OpenGLShader>(stage, source);
    }

    std::unique_ptr<RHIPipelineState> OpenGLDevice::CreatePipelineState(const RHIPipelineStateDesc &desc)
    {
        return std::make_unique<OpenGLPipeline>(desc);
    }

    std::unique_ptr<RHIFramebuffer> OpenGLDevice::CreateFramebuffer(const RHIFramebufferDesc &desc)
    {
        return std::make_unique<OpenGLFramebuffer>(desc);
    }

    RHICommandList *OpenGLDevice::GetCommandList()
    {
        return m_CommandList.get();
    }

    void OpenGLDevice::SubmitCommandList()
    {
        // OpenGL immediate mode: no submit required.
    }
}