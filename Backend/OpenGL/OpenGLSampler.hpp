#pragma once

#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>

namespace Physara::RHI
{
    class OpenGLSampler final : public RHISampler
    {
    public:
        explicit OpenGLSampler(const RHISamplerDesc &desc);
        ~OpenGLSampler() override;

        const RHISamplerDesc &GetDesc() const override { return m_Desc; }

        GLuint GetGLID() const { return m_ID; }

    private:
        GLuint m_ID{0};
        RHISamplerDesc m_Desc{};
    };
}