#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

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

    enum class DebugViewMode : std::uint32_t
    {
        None = 0,
        Normals = 1,
        Depth = 2
    };

    enum class ExposureMode : std::uint32_t
    {
        Manual = 0,
        Auto = 1
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

    enum class BloomMode : std::uint32_t
    {
        Legacy = 0,
        MipChain = 1,
        DualKawase = 2
    };

    struct PostProcessSettings
    {
        ToneMappingMode toneMappingMode{ToneMappingMode::ACES};
        bool bloomEnabled{true};
        AntiAliasingMode antiAliasingMode{AntiAliasingMode::FXAAQuality};
        BloomMode bloomMode{BloomMode::MipChain};
        DebugViewMode debugView{DebugViewMode::None};
        ExposureMode exposureMode{ExposureMode::Manual};
        float exposureCompensationEV{0.f};
        float bloomThreshold{1.0f};
        float bloomKnee{0.5f};
        float bloomIntensity{0.12f};
        float bloomRadius{2.0f};
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
        FrameStatistics *stats{nullptr};
        RHI::RHITexture *sceneHDR{nullptr};
        RHI::RHITexture *sceneDepth{nullptr};
        PostProcessSettings settings{};
    };

    class PostProcessPass final
    {
    public:
        void Execute(const PostProcessPassContext &context);

    private:
        void EnsureResources(const PostProcessPassContext &context);
        void EnsureBloomResources(const PostProcessPassContext &context);
        void ExecuteExposure(const PostProcessPassContext &context);
        void ExecuteBloom(const PostProcessPassContext &context);
        void ExecuteFullscreenPass(
            const PostProcessPassContext &context,
            RHI::RHIFramebuffer *framebuffer,
            const RHI::RHIRenderPassDesc &renderPassDesc,
            RHI::RHIPipelineState *pipeline,
            std::uint32_t width,
            std::uint32_t height,
            RHI::RHITexture *source0,
            RHI::RHITexture *source1);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const PostProcessPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetExposurePipeline(const PostProcessPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetBloomPipeline(const PostProcessPassContext &context, const char *debugName, const char *fragmentPath);
        [[nodiscard]] RHI::RHITexture *GetBloomTexture() const;
        [[nodiscard]] RHI::RHITexture *GetExposureTexture() const;

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

        std::unique_ptr<RHI::RHIBuffer> m_FrameBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_SettingsBuffer{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampSampler{};
        std::unique_ptr<RHI::RHITexture> m_BlackTexture{};
        std::unique_ptr<RHI::RHITexture> m_ExposureTexture{};
        std::unique_ptr<RHI::RHIFramebuffer> m_ExposureFramebuffer{};
        std::vector<BloomMip> m_BloomMips{};
        RHI::RHIRenderPassDesc m_ExposureRenderPassDesc{};
        RHI::RHIRenderPassDesc m_BloomRenderPassDesc{};
        std::uint32_t m_BloomWidth{0};
        std::uint32_t m_BloomHeight{0};
        std::uint64_t m_LastFrameUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastSettingsUploadSignature{std::numeric_limits<std::uint64_t>::max()};
    };
}