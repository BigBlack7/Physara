#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>
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

    struct SkyboxPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        std::filesystem::path environmentPath{};
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
        void UploadCubemap(const SkyboxPassContext &context, std::uint32_t faceSize, const std::vector<float> &pixels);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const SkyboxPassContext &context);

    private:
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHISampler> m_Sampler{};
        std::unique_ptr<RHI::RHITexture> m_SkyboxTexture{};
        std::filesystem::path m_LoadedEnvironmentPath{};
        bool m_LoggedPlaceholder{false};
    };
}