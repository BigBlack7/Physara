#include "OpenGLPipeline.hpp"

#include <cassert>
#include <string>

#include <Engine/Core/Log.hpp>

#include <Backend/OpenGL/OpenGLShader.hpp>

namespace Physara::RHI
{
    namespace Internal
    {
        static void GetVertexFormat(VertexFormat format, GLint &components, GLenum &type, GLboolean &normalized)
        {
            // RHI vertex format -> glVertexArrayAttribFormat参数
            // 目前只覆盖常用float/normalized byte格式, 整数属性后续需要glVertexArrayAttribIFormat
            switch (format)
            {
            case VertexFormat::RGBA32F:
                components = 4;
                type = GL_FLOAT;
                normalized = GL_FALSE;
                break;
            case VertexFormat::RGBA16F:
                components = 4;
                type = GL_HALF_FLOAT;
                normalized = GL_FALSE;
                break;
            case VertexFormat::RG16F:
                components = 2;
                type = GL_HALF_FLOAT;
                normalized = GL_FALSE;
                break;
            case VertexFormat::RGBA8:
                components = 4;
                type = GL_UNSIGNED_BYTE;
                normalized = GL_TRUE;
                break;
            case VertexFormat::R8:
                components = 1;
                type = GL_UNSIGNED_BYTE;
                normalized = GL_TRUE;
                break;
            default:
                assert(false && "Unsupported vertex attribute format.");
                components = 4;
                type = GL_FLOAT;
                normalized = GL_FALSE;
                break;
            }
        }

        static void LogProgramLinkError(GLuint program)
        {
            GLint len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

            std::string log;
            if (len > 0)
            {
                log.resize(static_cast<std::size_t>(len), '\0');
                glGetProgramInfoLog(program, len, nullptr, log.data());
            }
            else
            {
                log = "No info log from driver.";
            }

            PHYSARA_CORE_ERROR("Program link failed: {}", log);
        }
    }

    OpenGLPipeline::OpenGLPipeline(const RHIPipelineStateDesc &desc) : m_Desc(desc)
    {
        // OpenGL pipeline object由program + VAO + CommandList中的固定函数状态共同组成
        // 这里负责shader link和VAO vertex layout, raster/depth/blend在SetPipelineState时应用
        m_IsCompute = (m_Desc.computeShader != nullptr);
        if (m_IsCompute)
        {
            if (m_Desc.vertexShader != nullptr || m_Desc.fragmentShader != nullptr)
            {
                PHYSARA_CORE_ERROR("Compute pipeline should not have vertex/fragment shaders.");
            }
        }
        else
        {
            if (m_Desc.vertexShader == nullptr || m_Desc.fragmentShader == nullptr)
            {
                PHYSARA_CORE_ERROR("Graphics pipeline requires vertex + fragment shaders.");
            }
        }

        // Program是shader stage的链接产物. OpenGL没有独立PSO对象, 所以program只覆盖shader部分
        m_Program = glCreateProgram();
        if (m_Program == 0)
        {
            PHYSARA_CORE_ERROR("glCreateProgram failed.");
            return;
        }

        if (m_IsCompute)
        {
            auto *cs = static_cast<OpenGLShader *>(m_Desc.computeShader);
            if (cs && cs->IsValid())
            {
                glAttachShader(m_Program, cs->GetGLID());
            }
        }
        else
        {
            auto *vs = static_cast<OpenGLShader *>(m_Desc.vertexShader);
            auto *fs = static_cast<OpenGLShader *>(m_Desc.fragmentShader);

            if (vs && vs->IsValid())
            {
                glAttachShader(m_Program, vs->GetGLID());
            }
            if (fs && fs->IsValid())
            {
                glAttachShader(m_Program, fs->GetGLID());
            }
        }

        glLinkProgram(m_Program);

        GLint success = 0;
        glGetProgramiv(m_Program, GL_LINK_STATUS, &success);
        if (success == GL_FALSE)
        {
            Internal::LogProgramLinkError(m_Program);
            glDeleteProgram(m_Program);
            m_Program = 0;
            return;
        }

        if (!m_IsCompute)
        {
            // DSA VAO: 记录attribute format, attribute->binding映射和instance divisor
            // 具体vertex/index buffer object在CommandList::SetVertexBuffer/SetIndexBuffer绑定到此VAO
            glCreateVertexArrays(1, &m_VAO);

            for (const auto &attr : m_Desc.vertexAttributes)
            {
                GLint components = 0;
                GLenum type = GL_FLOAT;
                GLboolean normalized = GL_FALSE;

                Internal::GetVertexFormat(attr.format, components, type, normalized);

                glEnableVertexArrayAttrib(m_VAO, attr.location);
                glVertexArrayAttribFormat(
                    m_VAO,
                    attr.location,
                    components,
                    type,
                    normalized,
                    static_cast<GLuint>(attr.offset));

                glVertexArrayAttribBinding(m_VAO, attr.location, attr.binding);
            }

            for (const auto &binding : m_Desc.vertexBindings)
            {
                // instanceDivisor=0表示per-vertex; 1表示每实例前进一次, 适合instancing/MDI
                glVertexArrayBindingDivisor(m_VAO, binding.binding, binding.instanceDivisor);
            }
        }
    }

    OpenGLPipeline::~OpenGLPipeline()
    {
        if (m_VAO != 0)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }

        if (m_Program != 0)
        {
            glDeleteProgram(m_Program);
            m_Program = 0;
        }
    }
}