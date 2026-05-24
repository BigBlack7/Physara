#pragma once

#include <cstdint>
#include <limits>
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

    struct PostProcessSettings
    {
        bool toneMappingEnabled{true};
        bool bloomEnabled{true};
        bool fxaaEnabled{true};
        DebugViewMode debugView{DebugViewMode::None};
        ExposureMode exposureMode{ExposureMode::Manual};
        float exposureCompensationEV{0.f};
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
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const PostProcessPassContext &context);

    private:
        std::unique_ptr<RHI::RHIBuffer> m_FrameBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_SettingsBuffer{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampSampler{};
        std::uint64_t m_LastFrameUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastSettingsUploadSignature{std::numeric_limits<std::uint64_t>::max()};
    };
}