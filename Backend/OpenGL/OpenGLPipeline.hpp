#pragma once

#include <glad/glad.h>

#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::RHI
{
    class OpenGLPipeline final : public RHIPipelineState
    {
    public:
        explicit OpenGLPipeline(const RHIPipelineStateDesc &desc);
        ~OpenGLPipeline() override;

        bool IsValid() const override { return m_Program != 0; }

        GLuint GetProgram() const { return m_Program; }
        GLuint GetVAO() const { return m_VAO; }
        bool IsCompute() const { return m_IsCompute; }

        const RHIPipelineStateDesc &GetDesc() const { return m_Desc; }

    private:
        GLuint m_Program{0};
        GLuint m_VAO{0};
        RHIPipelineStateDesc m_Desc{};
        bool m_IsCompute{false};
    };
}