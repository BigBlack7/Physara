#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
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
    class GPUScene;
    class PipelineStateCache;
    class ShaderLibrary;

    enum class DebugViewMode : std::uint32_t
    {
        None = 0,
        Normals = 1,
        Depth = 2,
        Wireframe = 3,
        ShadowMap = 4,
        ShadowCascades = 5,
        LightClusters = 6,
        GBufferBaseColor = 7,
        GBufferNormal = 8,
        GBufferMaterial = 9,
        GBufferEmissive = 10
    };

    enum class ToneMappingMode : std::uint32_t
    {
        None = 0,
        ACES = 1,
        Reinhard = 2,
        Filmic = 3,
        Neutral = 4
    };

    enum class AntiAliasingMode : std::uint32_t
    {
        None = 0,
        FXAABasic = 1,
        FXAAQuality = 2,
        SMAALite = 3
    };

    struct PostProcessSettings
    {
        ToneMappingMode toneMappingMode{ToneMappingMode::ACES};
        bool bloomEnabled{true};
        AntiAliasingMode antiAliasingMode{AntiAliasingMode::FXAAQuality};
        DebugViewMode debugView{DebugViewMode::None};
        std::uint32_t shadowMapCascadeIndex{0u};
        float exposureCompensationEV{0.f};
        float bloomThreshold{1.0f};
        float bloomKnee{0.5f};
        float bloomIntensity{0.12f};
        float bloomScatter{0.7f};
        float aaSubpixel{0.75f};
        float aaEdgeThreshold{0.125f};
        float aaEdgeThresholdMin{0.0312f};
        float aaDepthSensitivity{24.f};
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
        FrameUploadAllocator *frameUploadAllocator{nullptr};
        const GPUScene *gpuScene{nullptr};
        FrameStatistics *stats{nullptr};
        RHI::RHITexture *sceneHDR{nullptr};
        RHI::RHITexture *sceneDepth{nullptr};
        RHI::RHITexture *shadowMap{nullptr};
        PostProcessSettings settings{};
    };

    class PostProcessPass final
    {
    public:
        void Execute(const PostProcessPassContext &context);

    private:
        void EnsureResources(const PostProcessPassContext &context);
        void EnsureBloomResources(const PostProcessPassContext &context);
        void ReleaseBloomResources(const PostProcessPassContext &context);
        void ExecuteBloom(const PostProcessPassContext &context, const FrameUploadAllocation &frameUniformAllocation);
        void ExecuteFullscreenPass(
            const PostProcessPassContext &context,
            const FrameUploadAllocation &frameUniformAllocation,
            RHI::RHIFramebuffer *framebuffer,
            const RHI::RHIRenderPassDesc &renderPassDesc,
            RHI::RHIPipelineState *pipeline,
            std::uint32_t width,
            std::uint32_t height,
            RHI::RHITexture *source0,
            RHI::RHITexture *source1);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const PostProcessPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetBloomPipeline(const PostProcessPassContext &context, const char *debugName, const char *fragmentPath);
        [[nodiscard]] RHI::RHITexture *GetBloomTexture() const;

    private:
        struct BloomMip
        {
            std::uint32_t width{0};
            std::uint32_t height{0};
            std::unique_ptr<RHI::RHITexture> downTexture{};
            std::unique_ptr<RHI::RHITexture> upTexture{};
            std::unique_ptr<RHI::RHIFramebuffer> downFramebuffer{};
            std::unique_ptr<RHI::RHIFramebuffer> upFramebuffer{};
        };

        FrameUploadAllocation m_SettingsAllocation{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampSampler{};
        std::unique_ptr<RHI::RHISampler> m_NearestClampSampler{};
        std::unique_ptr<RHI::RHITexture> m_BlackTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackShadowMap{};
        std::vector<BloomMip> m_BloomMips{};
        RHI::RHIRenderPassDesc m_BloomRenderPassDesc{};
        std::uint32_t m_BloomWidth{0};
        std::uint32_t m_BloomHeight{0};
    };
}