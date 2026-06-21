#pragma once

#include <cstdint>
#include <string_view>

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

    [[nodiscard]] constexpr std::string_view RenderPathName(RenderPath path)
    {
        switch (path)
        {
        case RenderPath::Forward:
            return "Forward";
        case RenderPath::ForwardPlus:
            return "Forward+";
        case RenderPath::Deferred:
            return "Deferred";
        default:
            return "Unknown";
        }
    }
}
