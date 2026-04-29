#pragma once

#include <string>

#include <glad/glad.h>

#include <Engine/RHI/Resource/RHIShader.hpp>

namespace Physara::RHI
{
    class OpenGLShader final : public RHIShader
    {
    public:
        OpenGLShader(ShaderStage stage, const std::string &source);
        ~OpenGLShader() override;

        ShaderStage GetStage() const override { return m_Stage; }
        bool IsValid() const override { return m_ID != 0; }

        GLuint GetGLID() const { return m_ID; }

    private:
        GLuint m_ID{0};
        ShaderStage m_Stage{ShaderStage::Vertex};
    };
}