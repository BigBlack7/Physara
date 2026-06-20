#pragma once

#include <cstdint>

namespace Physara::Engine
{
    enum class RenderPath : std::uint32_t
    {
        Forward = 0,
        ForwardPlus = 1,
        Deferred = 2
    };

    [[nodiscard]] constexpr bool UsesClusteredLighting(RenderPath path)
    {
        return path == RenderPath::ForwardPlus || path == RenderPath::Deferred;
    }
}