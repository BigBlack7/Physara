#pragma once

#include <memory>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHIFramebuffer;
    class RHIPipelineState;
    class RHISampler;
    class RHITexture;
    struct RHIRenderPassDesc;
}

namespace Physara::Engine
{
    class GPUScene;
    class IBLResources;
    class PipelineStateCache;
    class ShaderLibrary;

    struct DeferredLightingPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        const GPUScene *gpuScene{nullptr};
        FrameStatistics *stats{nullptr};
        RHI::RHITexture *sceneDepth{nullptr};
        RHI::RHITexture *baseColor{nullptr};
        RHI::RHITexture *normal{nullptr};
        RHI::RHITexture *material{nullptr};
        RHI::RHITexture *emissive{nullptr};
        RHI::RHITexture *shadowMap{nullptr};
        const IBLResources *iblResources{nullptr};
        glm::vec4 clearColor{0.f, 0.f, 0.f, 1.f};
    };

    class DeferredLightingPass final
    {
    public:
        void Execute(const DeferredLightingPassContext &context);
        void Reset();

    private:
        void EnsureResources(const DeferredLightingPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const DeferredLightingPassContext &context);

    private:
        std::unique_ptr<RHI::RHISampler> m_LinearClampSampler{};
        std::unique_ptr<RHI::RHISampler> m_NearestClampSampler{};
        std::unique_ptr<RHI::RHISampler> m_ShadowSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBlackCubeTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBRDFLut{};
    };
}