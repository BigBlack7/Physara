#include "OpenGLShader.hpp"

#include <string>

#include <Engine/Core/Log.hpp>
#include <Backend/OpenGL/OpenGLTypeMapping.hpp>

namespace Physara::RHI
{
    namespace OpenGLShaderDetail
    {
        static const char *ToStageName(ShaderStage stage)
        {
            switch (stage)
            {
            case ShaderStage::Vertex:
                return "VERTEX";
            case ShaderStage::Fragment:
                return "FRAGMENT";
            case ShaderStage::Compute:
                return "COMPUTE";
            default:
                return "UNKNOWN";
            }
        }
    }

    OpenGLShader::OpenGLShader(ShaderStage stage, const std::string &source) : m_Stage(stage)
    {
        // 这里只负责编译单个 shader stage；真正的 program link 在 OpenGLPipeline 中完成。
        const GLenum glStage = ToGLShaderStage(stage);
        m_ID = glCreateShader(glStage);

        if (m_ID == 0)
        {
            PHYSARA_CORE_ERROR("Shader create failed [{}]: glCreateShader returned 0", OpenGLShaderDetail::ToStageName(stage));
            return;
        }

        // glShaderSource 接收 C 字符串数组；源码生命周期只需覆盖此调用，驱动会复制内容。
        const char *src = source.c_str();
        glShaderSource(m_ID, 1, &src, nullptr);
        glCompileShader(m_ID);

        GLint success = 0;
        glGetShaderiv(m_ID, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE)
        {
            GLint len = 0;
            glGetShaderiv(m_ID, GL_INFO_LOG_LENGTH, &len);

            std::string log;
            if (len > 0)
            {
                log.resize(static_cast<std::size_t>(len), '\0');
                glGetShaderInfoLog(m_ID, len, nullptr, log.data());
            }
            else
            {
                log = "No info log from driver.";
            }

            PHYSARA_CORE_ERROR("Shader compile failed [{}]: {}", OpenGLShaderDetail::ToStageName(stage), log);

            glDeleteShader(m_ID);
            m_ID = 0;
        }
    }

    OpenGLShader::~OpenGLShader()
    {
        if (m_ID != 0)
        {
            glDeleteShader(m_ID);
            m_ID = 0;
        }
    }
}