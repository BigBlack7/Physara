#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Engine/Renderer/GPUContracts.hpp>
#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/RHI/Descriptors/RHIResourceSet.hpp>
#include <Engine/RHI/Resource/RHISampler.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

#include <glm/vec4.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    struct FrameData;
    struct FrameStatistics;
    struct MaterialComponent;

    inline constexpr std::size_t MaterialTextureSlotCount = 5u;

    struct MaterialResourceSet
    {
        MaterialInstanceId materialInstanceId{InvalidMaterialInstanceId};
        std::uint32_t textureSetId{0};
        std::array<RHI::RHITextureBinding, MaterialTextureSlotCount> textureBindings{};
        RHI::RHISampler *sampler{nullptr};
        bool complete{false};

        [[nodiscard]] RHI::RHIResourceSet AsRHIResourceSet() const
        {
            return RHI::RHIResourceSet{std::span<const RHI::RHITextureBinding>{textureBindings.data(), textureBindings.size()}};
        }
    };

    class MaterialTextureCache final
    {
    public:
        void Reset();

        void Update(
            RHI::RHIDevice &device,
            RHI::RHICommandList &commandList,
            AssetManager *assetManager,
            const FrameData &frameData,
            FrameStatistics *stats);

        [[nodiscard]] const MaterialResourceSet *GetResourceSet(std::uint32_t materialIndex) const;
        [[nodiscard]] bool HasBindlessTables() const { return m_BindlessReady; }
        [[nodiscard]] RHI::RHIBuffer *GetMaterialTextureIndexBuffer() const { return m_MaterialTextureIndexBuffer.get(); }
        [[nodiscard]] RHI::RHIBuffer *GetBindlessTextureHandleBuffer() const { return m_BindlessTextureHandleBuffer.get(); }

    private:
        struct alignas(16) MaterialTextureIndexData
        {
            glm::uvec4 slots0{0u};
            glm::uvec4 slots1{0u};
        };

        struct alignas(16) BindlessTextureHandleData
        {
            glm::uvec4 words{0u};
        };

        struct BindlessTextureHandleEntry
        {
            RHI::RHITexture *texture{nullptr};
            RHI::RHISampler *sampler{nullptr};
            std::uint32_t index{0};
        };

        struct TextureGPUResource
        {
            std::unique_ptr<RHI::RHITexture> texture{};
            bool generatedMipmaps{false};
        };

        struct TextureSetEntry
        {
            std::array<RHI::RHITexture *, MaterialTextureSlotCount> textures{};
            RHI::RHISampler *sampler{nullptr};
            std::uint32_t id{0};
        };

        void EnsureDefaults(RHI::RHIDevice &device);
        [[nodiscard]] RHI::RHITexture *GetOrCreateTexture(
            RHI::RHIDevice &device,
            RHI::RHICommandList &commandList,
            AssetManager *assetManager,
            const std::string &texturePath,
            RHI::TextureColorSpace colorSpace,
            FrameStatistics *stats);
        [[nodiscard]] const MaterialResourceSet *GetOrCreateResourceSet(
            RHI::RHIDevice &device,
            RHI::RHICommandList &commandList,
            AssetManager *assetManager,
            MaterialInstanceId materialInstanceId,
            const MaterialComponent &material,
            FrameStatistics *stats);
        [[nodiscard]] MaterialResourceSet BuildResourceSet(
            RHI::RHIDevice &device,
            RHI::RHICommandList &commandList,
            AssetManager *assetManager,
            MaterialInstanceId materialInstanceId,
            const MaterialComponent &material,
            FrameStatistics *stats);
        [[nodiscard]] std::uint32_t ResolveTextureSetId(
            const std::array<RHI::RHITextureBinding, MaterialTextureSlotCount> &textureBindings,
            RHI::RHISampler *sampler);
        void UpdateBindlessTables(RHI::RHIDevice &device, const FrameData &frameData, FrameStatistics *stats);
        [[nodiscard]] std::uint32_t ResolveBindlessTextureIndex(
            RHI::RHIDevice &device,
            RHI::RHITexture *texture,
            RHI::RHISampler *sampler);
        void UploadBindlessStorageBuffer(
            RHI::RHIDevice &device,
            std::unique_ptr<RHI::RHIBuffer> &buffer,
            const void *data,
            std::uint32_t size,
            FrameStatistics *stats);
        [[nodiscard]] std::uint64_t BuildBindlessUploadSignature() const;

    private:
        std::unique_ptr<RHI::RHISampler> m_LinearRepeatSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackWhiteTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackNormalTexture{};
        std::unique_ptr<RHI::RHIBuffer> m_MaterialTextureIndexBuffer{};
        std::unique_ptr<RHI::RHIBuffer> m_BindlessTextureHandleBuffer{};
        std::unordered_map<std::string, TextureGPUResource> m_TextureCache{};
        std::unordered_set<std::string> m_MissingTextureWarnings{};
        std::unordered_map<MaterialInstanceId, MaterialResourceSet> m_ResourceSets{};
        std::vector<const MaterialResourceSet *> m_FrameResourceSets{};
        std::vector<TextureSetEntry> m_TextureSets{};
        std::vector<MaterialTextureIndexData> m_FrameTextureIndices{};
        std::vector<BindlessTextureHandleData> m_BindlessTextureHandles{};
        std::vector<BindlessTextureHandleEntry> m_BindlessTextureHandleEntries{};
        std::uint32_t m_NextTextureSetId{1u};
        std::uint64_t m_ResourceSetFrameIndex{std::numeric_limits<std::uint64_t>::max()};
        std::uint64_t m_BindlessUploadSignature{std::numeric_limits<std::uint64_t>::max()};
        bool m_BindlessReady{false};
    };
}