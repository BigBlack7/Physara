#pragma once

#include <memory>

#include <Engine/RHI/Resource/RHIBuffer.hpp>

#include <glm/vec4.hpp>

namespace Physara::RHI
{
    class RHIDevice;
    class RHICommandList;
    class RHIFramebuffer;
    class RHIPipelineState;
    struct RHIRenderPassDesc;
}

namespace Physara::Engine
{
    class PipelineStateCache;
    class RenderProxy;
    class ShaderLibrary;
    struct FrameData;

    struct ForwardPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        const RenderProxy *renderProxy{nullptr};
        glm::vec4 clearColor{0.f, 0.f, 0.f, 1.f};
    };

    class ForwardOpaquePass final
    {
    public:
        void Execute(const ForwardPassContext &context);

    private:
        void EnsureFrameBuffers(const ForwardPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ForwardPassContext &context);

    private:
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_ObjectBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_LightBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_MaterialBuffer{};
    };
}