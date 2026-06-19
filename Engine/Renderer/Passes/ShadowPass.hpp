#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/RenderCommandExecutor.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHIFramebuffer;
    class RHIPipelineState;
}

namespace Physara::Engine
{
    class AssetManager;
    class PipelineStateCache;
    class RenderProxy;
    class ShaderLibrary;

    enum class ShadowFilter : std::uint32_t
    {
        Hard = GPUValue(ShadowFilterGPU::Hard),
        PCF3x3 = GPUValue(ShadowFilterGPU::PCF3x3),
        PCF5x5 = GPUValue(ShadowFilterGPU::PCF5x5),
        Poisson16 = GPUValue(ShadowFilterGPU::Poisson16),
        PCSS = GPUValue(ShadowFilterGPU::PCSS)
    };

    struct ShadowSettings
    {
        bool enabled{true};
        ShadowFilter filter{ShadowFilter::PCF3x3};
        std::uint32_t resolution{2048u};
        std::uint32_t cascadeCount{4u};
        float maxDistanceMeters{250.f};
        float splitLambda{0.7f};
        float transitionFraction{0.1f};
        float depthBias{2.f};
        float slopeBias{2.f};
        float normalBiasTexels{1.5f};
        float receiverBiasScale{1.f};
        float filterRadiusTexels{1.5f};
        float lightSizeTexels{24.f};
    };

    struct ShadowPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        FrameData *frameData{nullptr};
        FrameUploadAllocator *frameUploadAllocator{nullptr};
        GPUScene *gpuScene{nullptr};
        FrameStatistics *stats{nullptr};
        const RenderProxy *renderProxy{nullptr};
        MeshGPUCache *meshCache{nullptr};
        AssetManager *assetManager{nullptr};
    };

    class ShadowPass final
    {
    public:
        void Execute(const ShadowPassContext &context);
        void PrepareResources(RHI::RHIDevice &device);
        void Reset();
        void SetSettings(const ShadowSettings &settings);

        [[nodiscard]] RHI::RHITexture *GetShadowMap() const { return m_ShadowMap.get(); }
        [[nodiscard]] const ShadowSettings &GetSettings() const { return m_Settings; }

    private:
        struct CascadeState
        {
            CameraData camera{};
            FrameUploadAllocation cameraAllocation{};
            std::vector<RenderDrawItem> shadowCasters{};
            std::vector<RenderCommand> singleSidedCommands{};
            std::vector<RenderCommand> doubleSidedCommands{};
        };

        struct CommandSubmitContext
        {
            const ShadowPassContext *passContext{nullptr};
        };

        [[nodiscard]] bool BuildShadowData(const ShadowPassContext &context, std::uint32_t &lightIndex);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ShadowPassContext &context, RHI::CullMode cullMode);
        void BuildShadowCommands(const ShadowPassContext &context);
        void UploadFrameBuffers(const ShadowPassContext &context);
        void DrawShadowCasters(const ShadowPassContext &context, const std::vector<RenderCommand> &commands);
        static bool CanMergeShadowIndirectRun(void *userData, const RenderCommand &lhs, const RenderCommand &rhs);
        static void RecordSubmittedCommand(
            void *userData,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode);

    private:
        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_ShadowMap{};
        std::array<std::unique_ptr<RHI::RHIFramebuffer>, MaxShadowCascades> m_Framebuffers{};
        std::array<CascadeState, MaxShadowCascades> m_Cascades{};
        std::vector<std::uint32_t> m_ShadowInstanceObjectIndexScratch{};
        RenderCommandExecutor m_CommandExecutor{};
        ShadowSettings m_Settings{};
    };
}
