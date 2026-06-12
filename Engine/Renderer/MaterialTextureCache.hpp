#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    struct MaterialTextureBinding
    {
        std::array<RHI::RHITexture *, 5> textures{};
        RHI::RHISampler *sampler{nullptr};
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

        [[nodiscard]] const MaterialTextureBinding *GetBinding(std::uint32_t materialIndex) const;

    private:
        struct TextureGPUResource
        {
            std::unique_ptr<RHI::RHITexture> texture{};
            bool generatedMipmaps{false};
        };

        void EnsureDefaults(RHI::RHIDevice &device);
        [[nodiscard]] RHI::RHITexture *GetOrCreateTexture(
            RHI::RHIDevice &device,
            RHI::RHICommandList &commandList,
            AssetManager *assetManager,
            const std::string &texturePath,
            RHI::TextureColorSpace colorSpace,
            FrameStatistics *stats);

    private:
        std::unique_ptr<RHI::RHISampler> m_LinearRepeatSampler{};
        std::unique_ptr<RHI::RHITexture> m_FallbackWhiteTexture{};
        std::unique_ptr<RHI::RHITexture> m_FallbackNormalTexture{};
        std::unordered_map<std::string, TextureGPUResource> m_TextureCache{};
        std::unordered_set<std::string> m_MissingTextureWarnings{};
        std::vector<MaterialTextureBinding> m_Bindings{};
        std::uint64_t m_TextureBindingFrameIndex{0};
    };
}