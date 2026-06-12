#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

#include <glm/vec4.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHIFramebuffer;
    class RHIPipelineState;
}

namespace Physara::Engine
{
    class PipelineStateCache;
    class ShaderLibrary;
    struct Texture;

    struct SkyboxPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        FrameUploadAllocator *frameUploadAllocator{nullptr};
        FrameStatistics *stats{nullptr};
        std::filesystem::path environmentPath{};
        float exposureCompensation{0.f};
        bool enabled{true};
    };

    class SkyboxPass final
    {
    public:
        void Execute(const SkyboxPassContext &context);
        void InvalidateEnvironment();

    private:
        void EnsureResources(const SkyboxPassContext &context);
        void EnsureSkyboxTexture(const SkyboxPassContext &context);
        void UploadPanorama(const SkyboxPassContext &context, const Texture &panorama);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const SkyboxPassContext &context);

    private:
        std::unique_ptr<RHI::RHISampler> m_Sampler{};
        std::unique_ptr<RHI::RHITexture> m_SkyboxTexture{};
        std::filesystem::path m_LoadedEnvironmentPath{};
        bool m_LoggedPlaceholder{false};
    };
}