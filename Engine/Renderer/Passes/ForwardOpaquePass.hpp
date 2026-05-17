#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Engine/Renderer/RenderProxy.hpp>
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
        AssetManager *assetManager{nullptr};
        glm::vec4 clearColor{0.f, 0.f, 0.f, 1.f};
    };

    class ForwardOpaquePass final
    {
    public:
        void Execute(const ForwardPassContext &context);

    private:
        struct MeshGPUPrimitive
        {
            std::unique_ptr<RHI::RHIBuffer> vertexBuffer{};
            std::unique_ptr<RHI::RHIBuffer> indexBuffer{};
            std::uint32_t indexCount{0};
        };

        struct TextureGPUResource
        {
            std::unique_ptr<RHI::RHITexture> texture{};
            bool generatedMipmaps{false};
        };

        void EnsureFrameBuffers(const ForwardPassContext &context);
        void EnsureDefaultTextures(const ForwardPassContext &context);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ForwardPassContext &context, RHI::CullMode cullMode);
        [[nodiscard]] MeshGPUPrimitive *GetOrCreateMeshPrimitive(const ForwardPassContext &context, const RenderDrawItem &item);
        [[nodiscard]] RHI::RHITexture *GetOrCreateTexture(const ForwardPassContext &context, const std::string &texturePath);
        [[nodiscard]] RHI::RHITexture *GetFallbackWhiteTexture() const;
        [[nodiscard]] RHI::RHITexture *GetFallbackNormalTexture() const;
        void BindMaterial(const ForwardPassContext &context, const RenderDrawItem &item);
        void DrawBucket(const ForwardPassContext &context, const std::vector<RenderDrawItem> &bucket, bool drawDoubleSided);
        [[nodiscard]] bool CanInstanceTogether(const ForwardPassContext &context, const RenderDrawItem &first, const RenderDrawItem &candidate, std::uint32_t instanceOffset);
        void ResetTextureBindings();
        [[nodiscard]] static std::string BuildMeshResourcePath(const RenderDrawItem &item);
        [[nodiscard]] static std::string BuildMeshPrimitiveDebugName(const RenderDrawItem &item);

    private:
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_ObjectBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_LightBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_MaterialBuffer{};
        std::unique_ptr<RHI::RHISampler> m_LinearRepeatSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackWhiteTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackNormalTexture{};
        std::unordered_map<std::uint64_t, MeshGPUPrimitive> m_MeshCache{};
        std::unordered_map<std::string, TextureGPUResource> m_TextureCache{};
        std::unordered_set<std::uint64_t> m_MissingMeshWarnings{};
        std::unordered_set<std::string> m_MissingTextureWarnings{};
        RHI::RHITexture *m_BoundTextures[5]{};
        RHI::RHISampler *m_BoundSampler{nullptr};
        bool m_LoggedFirstScene{false};
        bool m_LoggedFirstDraw{false};
    };
}