#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct RenderView;

    class FrustumPartition final
    {
    public:
        [[nodiscard]] static std::vector<float> BuildPracticalDepthSplits(
            float nearDistance,
            float farDistance,
            std::uint32_t partitionCount,
            float logarithmicWeight);

        [[nodiscard]] static std::array<glm::vec3, 8> BuildSliceCorners(
            const RenderView &view,
            float sliceNearDistance,
            float sliceFarDistance);
    };
}