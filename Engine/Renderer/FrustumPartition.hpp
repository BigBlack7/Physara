#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct RenderView;

    struct LogarithmicDepthPartition
    {
        float nearDistance{0.1f};
        float farDistance{1000.f};
        float sliceScale{1.f};
        float sliceBias{0.f};
        std::uint32_t sliceCount{1u};

        [[nodiscard]] std::uint32_t GetSlice(float depth) const;
    };

    class FrustumPartition final
    {
    public:
        [[nodiscard]] static LogarithmicDepthPartition BuildLogarithmicDepthPartition(
            float nearDistance,
            float farDistance,
            std::uint32_t partitionCount);

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