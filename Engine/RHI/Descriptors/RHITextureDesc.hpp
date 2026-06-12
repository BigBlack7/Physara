#pragma once

#include <cstdint>

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    enum class TextureColorSpace : std::uint8_t
    {
        Linear = 0,
        SRGB = 1
    };

    struct RHITextureDesc
    {
        std::uint32_t width{1};
        std::uint32_t height{1};
        std::uint32_t depth{1};
        std::uint32_t mipLevels{1};
        std::uint32_t arrayLayers{1};
        std::uint32_t samples{1};
        TextureFormat format{TextureFormat::RGBA8};
        TextureColorSpace colorSpace{TextureColorSpace::Linear};
        TextureDimension dimension{TextureDimension::Tex2D};
        TextureUsageFlags usage{TextureUsage::Sampled};
        const void *initialData{nullptr};
    };
}