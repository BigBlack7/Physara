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
    struct RenderMeshSubmission;

    enum class ShadowAlgorithm : std::uint32_t
    {
        None = 0,
        SingleMapHard = 1,
        SingleMapPCF3x3 = 2,
        SingleMapPCF5x5 = 3,
        SingleMapPoisson16 = 4,
        SingleMapPCSS = 5
    };

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
        void Reset();
        void SetSettings(const ShadowSettings &settings);

        [[nodiscard]] RHI::RHITexture *GetShadowMap() const { return m_ShadowMap.get(); }
        [[nodiscard]] const ShadowSettings &GetSettings() const { return m_Settings; }

    private:
        void EnsureResources(const ShadowPassContext &context);
        [[nodiscard]] bool BuildShadowData(
            const ShadowPassContext &context,
            CameraData &shadowCamera,
            std::uint32_t &lightIndex);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ShadowPassContext &context);
        void BuildShadowBatches(const ShadowPassContext &context);
        void UploadFrameBuffers(const ShadowPassContext &context, const CameraData &shadowCamera);
        void DrawShadowCasters(const ShadowPassContext &context);

    private:
        struct ShadowDrawBatch
        {
            const RenderMeshSubmission *submission{nullptr};
            std::uint32_t firstItem{0};
            std::uint32_t itemCount{0};
            std::uint32_t firstObjectIndex{0};
            std::uint32_t firstInstanceIndex{0};
            std::uint64_t meshKey{0};
            std::uint64_t primitiveKey{0};
        };

        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_ShadowMap{};
        std::unique_ptr<RHI::RHIFramebuffer> m_Framebuffer{};
        FrameUploadAllocation m_CameraAllocation{};
        std::vector<RenderDrawItem> m_ShadowCasterScratch{};
        std::vector<ShadowDrawBatch> m_ShadowBatchScratch{};
        std::vector<std::uint32_t> m_ShadowInstanceObjectIndexScratch{};
        ShadowSettings m_Settings{};
    };
}