#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/RenderCommandExecutor.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>
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

    enum class ShadowAlgorithm : std::uint32_t
    {
        None = 0,
        SingleMapHard = 1,
        SingleMapPCF3x3 = 2,
        SingleMapPCF5x5 = 3,
        SingleMapPoisson16 = 4,
        SingleMapPCSS = 5
    };

    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::None) == GPUValue(ShadowModeGPU::None));
    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::SingleMapHard) == GPUValue(ShadowModeGPU::Hard));
    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::SingleMapPCF3x3) == GPUValue(ShadowModeGPU::PCF3x3));
    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::SingleMapPCF5x5) == GPUValue(ShadowModeGPU::PCF5x5));
    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::SingleMapPoisson16) == GPUValue(ShadowModeGPU::Poisson16));
    static_assert(static_cast<std::uint32_t>(ShadowAlgorithm::SingleMapPCSS) == GPUValue(ShadowModeGPU::PCSS));

    struct ShadowSettings
    {
        ShadowAlgorithm algorithm{ShadowAlgorithm::SingleMapPCF3x3};
        std::uint32_t resolution{2048u};
        float depthBias{2.f};
        float slopeBias{2.f};
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
        struct CommandSubmitContext
        {
            const ShadowPassContext *passContext{nullptr};
        };

        [[nodiscard]] bool BuildShadowData(
            const ShadowPassContext &context,
            CameraData &shadowCamera,
            std::uint32_t &lightIndex);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ShadowPassContext &context);
        void BuildShadowCommands(const ShadowPassContext &context);
        void UploadFrameBuffers(const ShadowPassContext &context, const CameraData &shadowCamera);
        void DrawShadowCasters(const ShadowPassContext &context);
        static bool CanMergeShadowIndirectRun(void *userData, const RenderCommand &lhs, const RenderCommand &rhs);
        static void RecordSubmittedCommand(
            void *userData,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode);

    private:
        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_ShadowMap{};
        std::unique_ptr<RHI::RHIFramebuffer> m_Framebuffer{};
        FrameUploadAllocation m_CameraAllocation{};
        std::vector<RenderDrawItem> m_ShadowCasterScratch{};
        std::vector<RenderCommand> m_ShadowCommandScratch{};
        std::vector<std::uint32_t> m_ShadowInstanceObjectIndexScratch{};
        RenderCommandExecutor m_CommandExecutor{};
        ShadowSettings m_Settings{};
    };
}