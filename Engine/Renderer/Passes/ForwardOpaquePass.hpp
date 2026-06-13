#pragma once

#include <array>
#include <memory>
#include <limits>
#include <span>
#include <vector>

#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/Renderer/MaterialTextureCache.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/RHI/Descriptors/RHIIndirectDrawCommand.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>
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
        struct IndirectRun
        {
            std::uint32_t commandIndex{0};
            std::uint32_t commandCount{0};
            std::uint32_t indirectCommandOffset{0};
            std::uint32_t primitiveScratchOffset{0};
            RHI::RHIBuffer *indirectBuffer{nullptr};
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
        void BuildIndirectRuns(const ForwardPassContext &context, std::span<const RenderCommand> commands);
        [[nodiscard]] RHI::RHIBuffer *UploadIndirectCommands(const ForwardPassContext &context);
        void SubmitDirectCommand(const ForwardPassContext &context, const RenderCommand &command, bool transparent);
        void SubmitIndirectRun(
            const ForwardPassContext &context,
            std::span<const RenderCommand> commands,
            const IndirectRun &run,
            bool transparent);
        void RecordSubmittedCommand(const ForwardPassContext &context, const MeshGPUPrimitive &primitive, std::uint32_t instanceCount, bool transparent);
        void ResetTextureBindings();

    private:
        MaterialTextureCache m_MaterialTextureCache{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampMipSampler{};
        std::unique_ptr<RHI::RHISampler> m_ShadowSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBlackCubeTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBRDFLut{};
        std::vector<std::unique_ptr<RHI::RHIBuffer>> m_IndirectCommandBuffers{};
        std::vector<RHI::RHIDrawIndexedIndirectCommand> m_IndirectCommandScratch{};
        std::vector<MeshGPUPrimitive *> m_IndirectPrimitiveScratch{};
        std::vector<IndirectRun> m_IndirectRuns{};
        MaterialInstanceId m_BoundMaterialInstanceId{InvalidMaterialInstanceId};
        std::uint32_t m_IndirectBufferCursor{0};
        bool m_LoggedFirstScene{false};
        bool m_LoggedFirstDraw{false};
    };
}