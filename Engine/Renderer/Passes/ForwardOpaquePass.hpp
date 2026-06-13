#pragma once

#include <array>
#include <memory>
#include <span>

#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/Renderer/MaterialTextureCache.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/RenderCommandExecutor.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>
#include <Engine/RHI/RHIDefinitions.hpp>

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
    class AssetManager;
    class IBLResources;
    class PipelineStateCache;
    class RenderProxy;
    class ShaderLibrary;
    struct FrameData;
    struct ShadowData;

    struct ForwardPassContext
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
        RHI::RHITexture *shadowMap{nullptr};
        const IBLResources *iblResources{nullptr};
        float environmentExposureCompensation{0.f};
        glm::vec4 clearColor{0.f, 0.f, 0.f, 1.f};
        std::uint32_t debugView{0};
    };

    class ForwardOpaquePass final
    {
    public:
        void Execute(const ForwardPassContext &context);
        void ExecuteTransparent(const ForwardPassContext &context);
        void Reset();

    private:
        struct CommandSubmitContext
        {
            ForwardOpaquePass *pass{nullptr};
            const ForwardPassContext *passContext{nullptr};
            bool transparent{false};
        };

        void EnsureDefaultTextures(const ForwardPassContext &context);
        void ExecuteBuckets(const ForwardPassContext &context, bool transparent);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ForwardPassContext &context, RHI::CullMode cullMode, bool transparent);
        void BindFrameTextures(const ForwardPassContext &context);
        [[nodiscard]] bool BindMaterial(const ForwardPassContext &context, const RenderCommand &command);
        void DrawCommandGroup(
            const ForwardPassContext &context,
            RHI::RHIPipelineState *pipeline,
            std::span<const RenderCommand> primaryCommands,
            std::span<const RenderCommand> secondaryCommands,
            bool transparent);
        void DrawCommands(const ForwardPassContext &context, std::span<const RenderCommand> commands, bool transparent);
        static bool BindSubmittedCommand(void *userData, const RenderCommand &command);
        static void RecordSubmittedCommand(
            void *userData,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode);
        void RecordSubmittedCommand(
            const ForwardPassContext &context,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode,
            bool transparent);
        void ResetTextureBindings();

    private:
        MaterialTextureCache m_MaterialTextureCache{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampMipSampler{};
        std::unique_ptr<RHI::RHISampler> m_ShadowSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBlackCubeTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBRDFLut{};
        RenderCommandExecutor m_CommandExecutor{};
        MaterialInstanceId m_BoundMaterialInstanceId{InvalidMaterialInstanceId};
        bool m_LoggedFirstScene{false};
        bool m_LoggedFirstDraw{false};
    };
}