#pragma once

#include <array>
#include <memory>
#include <limits>
#include <vector>

#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MaterialTextureCache.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
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
        FrameUploadAllocator *frameUploadAllocator{nullptr};
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
        void EnsureFrameBuffers(const ForwardPassContext &context);
        void EnsureDefaultTextures(const ForwardPassContext &context);
        void ExecuteBuckets(const ForwardPassContext &context, bool transparent);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ForwardPassContext &context, RHI::CullMode cullMode, bool transparent);
        void BindFrameState(const ForwardPassContext &context);
        void BindMaterial(const ForwardPassContext &context, const RenderDrawBatch &batch);
        void DrawBatches(const ForwardPassContext &context, const std::vector<RenderDrawBatch> &batches, bool drawDoubleSided, bool transparent);
        void ResetTextureBindings();

    private:
        FrameUploadAllocation m_CameraAllocation{};
        FrameUploadAllocation m_RenderSettingsAllocation{};
        FrameUploadAllocation m_ShadowAllocation{};
        FrameUploadAllocation m_IBLAllocation{};
        MaterialTextureCache m_MaterialTextureCache{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampMipSampler{};
        std::unique_ptr<RHI::RHISampler> m_ShadowSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBlackCubeTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBRDFLut{};
        RHI::RHITexture *m_BoundTextures[5]{};
        RHI::RHISampler *m_BoundSampler{nullptr};
        std::uint32_t m_BoundMaterialIndex{std::numeric_limits<std::uint32_t>::max()};
        std::uint64_t m_LastUploadedFrameIndex{std::numeric_limits<std::uint64_t>::max()};
        bool m_LoggedFirstScene{false};
        bool m_LoggedFirstDraw{false};
    };
}