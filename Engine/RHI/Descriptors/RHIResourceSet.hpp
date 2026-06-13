#pragma once

#include <cstdint>
#include <span>

namespace Physara::RHI
{
    class RHISampler;
    class RHITexture;

    struct RHITextureBinding
    {
        std::uint32_t slot{0};
        RHITexture *texture{nullptr};
        RHISampler *sampler{nullptr};
    };

    struct RHIResourceSet
    {
        std::span<const RHITextureBinding> textures{};
    };
}