#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>

#include <Engine/Renderer/FrameData.hpp>
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
        void UploadFrameBuffers(const ShadowPassContext &context, const CameraData &shadowCamera);
        void DrawShadowCasters(const ShadowPassContext &context);

    private:
        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_ShadowMap{};
        std::unique_ptr<RHI::RHIFramebuffer> m_Framebuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_ObjectBuffer{};
        std::vector<RenderDrawItem> m_ShadowCasterScratch{};
        std::vector<glm::mat4> m_ObjectUploadScratch{};
        std::uint64_t m_LastCameraUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        ShadowSettings m_Settings{};
    };
}