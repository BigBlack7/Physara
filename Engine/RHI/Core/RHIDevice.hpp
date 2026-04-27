#pragma once

#include <memory>
#include <string>

#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>

#include <Engine/RHI/Resource/RHIBuffer.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>
#include <Engine/RHI/Resource/RHIShader.hpp>

#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

#include <Engine/RHI/Command/RHICommandList.hpp>

namespace Physara::RHI
{
    class RHIDevice
    {
    public:
        virtual ~RHIDevice() = default;

        // 生命周期
        virtual bool Init(void *windowHandle) = 0;
        virtual void Shutdown() = 0;

        // 资源创建(调用方持有所有权)
        virtual std::unique_ptr<RHIBuffer> CreateBuffer(const RHIBufferDesc &desc) = 0;
        virtual std::unique_ptr<RHITexture> CreateTexture(const RHITextureDesc &desc) = 0;
        virtual std::unique_ptr<RHISampler> CreateSampler(const RHISamplerDesc &desc) = 0;
        virtual std::unique_ptr<RHIShader> CreateShader(ShaderStage stage, const std::string &source) = 0;
        virtual std::unique_ptr<RHIPipelineState> CreatePipelineState(const RHIPipelineStateDesc &desc) = 0;
        virtual std::unique_ptr<RHIFramebuffer> CreateFramebuffer(const RHIFramebufferDesc &desc) = 0;

        // CommandList(Device持有命令列表对象)
        virtual RHICommandList *GetCommandList() = 0;
        virtual void SubmitCommandList() = 0;

        // 查询
        virtual int GetMaxAnisotropy() const = 0; // 纹理各向异性上限, Sampler创建时用
    };
}