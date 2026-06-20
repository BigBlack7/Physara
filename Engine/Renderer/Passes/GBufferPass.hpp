#pragma once

#include <cstdint>
#include <span>

#include <Engine/Renderer/MaterialTextureCache.hpp>
#include <Engine/Renderer/RenderCommandExecutor.hpp>

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
    class AssetManager;
    class GPUScene;
    class MeshGPUCache;
    class PipelineStateCache;
    class RenderProxy;
    class ShaderLibrary;
    struct FrameData;
    struct FrameStatistics;

    struct GBufferPassContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        RHI::RHIFramebuffer *framebuffer{nullptr};
        const RHI::RHIRenderPassDesc *renderPassDesc{nullptr};
        ShaderLibrary *shaderLibrary{nullptr};
        PipelineStateCache *pipelineCache{nullptr};
        const FrameData *frameData{nullptr};
        const GPUScene *gpuScene{nullptr};
        FrameStatistics *stats{nullptr};
        const RenderProxy *renderProxy{nullptr};
        MeshGPUCache *meshCache{nullptr};
        AssetManager *assetManager{nullptr};
        MaterialTextureCache *materialTextureCache{nullptr};
        bool wireframe{false};
    };

    class GBufferPass final
    {
    public:
        void Execute(const GBufferPassContext &context);
        void Reset();

    private:
        struct CommandSubmitContext
        {
            GBufferPass *pass{nullptr};
            const GBufferPassContext *context{nullptr};
        };

        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const GBufferPassContext &context, RHI::CullMode cullMode);
        [[nodiscard]] bool BindMaterial(const GBufferPassContext &context, const RenderCommand &command);
        [[nodiscard]] bool CanMerge(const GBufferPassContext &context, const RenderCommand &lhs, const RenderCommand &rhs) const;
        void DrawCommands(const GBufferPassContext &context, std::span<const RenderCommand> commands);
        static bool BindSubmittedCommand(void *userData, const RenderCommand &command);
        static bool CanMergeSubmittedCommands(void *userData, const RenderCommand &lhs, const RenderCommand &rhs);
        static void RecordSubmittedCommand(
            void *userData,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode);

    private:
        RenderCommandExecutor m_CommandExecutor{};
        std::uint32_t m_BoundMaterialTextureSetId{0};
    };
}