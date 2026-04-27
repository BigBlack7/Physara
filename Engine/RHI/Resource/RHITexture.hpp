#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    class RHITexture
    {
    public:
        virtual ~RHITexture() = default;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;
        virtual std::uint32_t GetMipLevels() const = 0;
        virtual std::uint32_t GetArrayLayers() const = 0;
        virtual TextureFormat GetFormat() const = 0;
        virtual TextureDimension GetDimension() const = 0;
        virtual TextureUsageFlags GetUsage() const = 0;
    };
}