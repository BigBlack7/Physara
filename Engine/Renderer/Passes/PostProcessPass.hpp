#pragma once

#include <memory>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHIFramebuffer;
    class RHIPipelineState;
    class RHITexture;
}

namespace Physara::Engine
{
    class PipelineStateCache;
    class ShaderLibrary;

    struct PostProcessSettings
    {
        bool toneMappingEnabled{true};
        bool bloomEnabled{true};
        bool fxaaEnabled{true};
        float bloomThreshold{1.0f};
        float bloomKnee{0.5f};
        float bloomIntensity{0.08f};
        float bloomRadius{2.0f};
    };

    struct PostProcessPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        RHI::RHITexture *sceneHDR{nullptr};
        PostProcessSettings settings{};
    };

    class PostProcessPass final
    {
    public:
        void Execute(const PostProcessPassContext &context);

    private:
        void EnsureResources(const PostProcessPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const PostProcessPassContext &context);

    private:
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_SettingsBuffer{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampSampler{};
    };
}