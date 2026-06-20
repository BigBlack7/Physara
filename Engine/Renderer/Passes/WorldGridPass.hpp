#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHIFramebuffer;
    class RHIPipelineState;
    struct RHIRenderPassDesc;
}

namespace Physara::Engine
{
    class GPUScene;
    class PipelineStateCache;
    class ShaderLibrary;

    struct WorldGridSettings
    {
        bool enabled{true};
        float minorSpacingMeters{1.f};
        std::uint32_t majorLineInterval{10u};
        float fadeStartMeters{50.f};
        float fadeEndMeters{500.f};
    };

    struct WorldGridPassContext
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
        WorldGridSettings settings{};
    };

    class WorldGridPass final
    {
    public:
        void Execute(const WorldGridPassContext &context);

    private:
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const WorldGridPassContext &context);
    };
}