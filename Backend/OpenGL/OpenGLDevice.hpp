#pragma once

#include <memory>

#include <Engine/RHI/Core/RHIDevice.hpp>

namespace Physara::RHI
{
    class OpenGLCommandList;

    class OpenGLDevice final : public RHIDevice
    {
    public:
        OpenGLDevice();
        ~OpenGLDevice() override;

        bool Init(void *windowHandle) override;
        void Shutdown() override;

        std::unique_ptr<RHIBuffer> CreateBuffer(const RHIBufferDesc &desc) override;
        std::unique_ptr<RHITexture> CreateTexture(const RHITextureDesc &desc) override;
        std::unique_ptr<RHISampler> CreateSampler(const RHISamplerDesc &desc) override;
        std::unique_ptr<RHIShader> CreateShader(ShaderStage stage, const std::string &source) override;
        std::unique_ptr<RHIPipelineState> CreatePipelineState(const RHIPipelineStateDesc &desc) override;
        std::unique_ptr<RHIFramebuffer> CreateFramebuffer(const RHIFramebufferDesc &desc) override;

        RHICommandList *GetCommandList() override;
        void SubmitCommandList() override;
        void WaitIdle() override;

        int GetMaxAnisotropy() const override { return m_MaxAnisotropy; }
        const char *GetBackendName() const override { return "OpenGL 4.6"; }

    private:
        std::unique_ptr<OpenGLCommandList> m_CommandList{};
        int m_MaxAnisotropy{1};
    };
}