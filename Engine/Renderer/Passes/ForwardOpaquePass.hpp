#pragma once

#include <array>
#include <memory>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Engine/Renderer/MeshGPUCache.hpp>
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

    struct alignas(16) ForwardMaterialGPUData
    {
        glm::vec4 baseColor{1.f};
        glm::vec4 emissiveColorLuminance{0.f, 0.f, 0.f, 0.f};
        glm::vec4 metallicRoughnessReflectanceAO{0.f, 0.5f, 0.5f, 1.f};
        glm::vec4 alphaNormalFlags{0.5f, 1.f, 0.f, 0.f};
        glm::vec4 textureFlags{0.f, 0.f, 0.f, 0.f};
        glm::vec4 textureCoordSets{0.f, 0.f, 0.f, 0.f};
        glm::vec4 materialFlags{0.f, 0.f, 0.f, 0.f};
        glm::vec4 textureInfluences{1.f, 1.f, 1.f, 0.f};
    };

    static_assert(sizeof(ForwardMaterialGPUData) % 16 == 0);

    class ForwardOpaquePass final
    {
    public:
        void Execute(const ForwardPassContext &context);
        void ExecuteTransparent(const ForwardPassContext &context);

    private:
        struct TextureGPUResource
        {
            std::unique_ptr<RHI::RHITexture> texture{};
            bool generatedMipmaps{false};
        };

        struct MaterialTextureBinding
        {
            std::array<RHI::RHITexture *, 5> textures{};
            RHI::RHISampler *sampler{nullptr};
        };

        void EnsureFrameBuffers(const ForwardPassContext &context);
        void EnsureDefaultTextures(const ForwardPassContext &context);
        void EnsureMaterialTextureBindings(const ForwardPassContext &context);
        void ExecuteBuckets(const ForwardPassContext &context, bool transparent);
        [[nodiscard]] RHI::RHIPipelineState *GetPipeline(const ForwardPassContext &context, RHI::CullMode cullMode, bool transparent);
        [[nodiscard]] RHI::RHITexture *GetOrCreateTexture(const ForwardPassContext &context, const std::string &texturePath);
        [[nodiscard]] RHI::RHITexture *GetFallbackWhiteTexture() const;
        [[nodiscard]] RHI::RHITexture *GetFallbackNormalTexture() const;
        void BindFrameState(const ForwardPassContext &context);
        void BindMaterial(const ForwardPassContext &context, const RenderDrawItem &item);
        void DrawBucket(const ForwardPassContext &context, const std::vector<RenderDrawItem> &bucket, bool drawDoubleSided);
        [[nodiscard]] bool CanInstanceTogether(const ForwardPassContext &context, const RenderDrawItem &first, const RenderDrawItem &candidate, std::uint32_t instanceOffset);
        void ResetTextureBindings();

    private:
        std::unique_ptr<RHI::RHIBuffer> m_CameraBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_ObjectBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_LightBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_MaterialBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_RenderSettingsBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_ShadowBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_IBLBuffer{};
        std::unique_ptr<RHI::RHISampler> m_LinearRepeatSampler{};
        std::unique_ptr<RHI::RHISampler> m_LinearClampMipSampler{};
        std::unique_ptr<RHI::RHISampler> m_ShadowSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackWhiteTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackNormalTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBlackCubeTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackBRDFLut{};
        std::unordered_map<std::string, TextureGPUResource> m_TextureCache{};
        std::unordered_set<std::string> m_MissingTextureWarnings{};
        std::vector<ForwardMaterialGPUData> m_MaterialUploadScratch{};
        std::vector<MaterialTextureBinding> m_MaterialTextureBindings{};
        RHI::RHITexture *m_BoundTextures[5]{};
        RHI::RHISampler *m_BoundSampler{nullptr};
        std::uint64_t m_LastUploadedFrameIndex{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_TextureBindingFrameIndex{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastCameraUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastMaterialUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastRenderSettingsUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastShadowUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_LastIBLUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        bool m_LoggedFirstScene{false};
        bool m_LoggedFirstDraw{false};
    };
}