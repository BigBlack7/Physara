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
#include <Engine/RHI/Resource/RHITexture.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

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

    private:
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

    private:
        std::unique_ptr<RHI::RHISampler> m_LinearRepeatSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackWhiteTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackNormalTexture{};
        std::unordered_map<std::string, TextureGPUResource> m_TextureCache{};
        std::unordered_set<std::string> m_MissingTextureWarnings{};
        std::unordered_map<MaterialInstanceId, MaterialResourceSet> m_ResourceSets{};
        std::vector<const MaterialResourceSet *> m_FrameResourceSets{};
        std::vector<TextureSetEntry> m_TextureSets{};
        std::uint32_t m_NextTextureSetId{1u};
        std::uint64_t m_ResourceSetFrameIndex{std::numeric_limits<std::uint64_t>::max()};
    };
}