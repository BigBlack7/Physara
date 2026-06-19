#include "FrustumPartition.hpp"

#include <algorithm>
#include <cmath>

#include <glm/vec4.hpp>

#include <Engine/Renderer/RenderView.hpp>

namespace Physara::Engine
{
    std::vector<float> FrustumPartition::BuildPracticalDepthSplits(
        float nearDistance,
        float farDistance,
        std::uint32_t partitionCount,
        float logarithmicWeight)
    {
        const float safeNear = std::max(nearDistance, 0.001f);
        const float safeFar = std::max(farDistance, safeNear + 0.001f);
        const std::uint32_t safeCount = std::max(partitionCount, 1u);
        const float lambda = std::clamp(logarithmicWeight, 0.f, 1.f);

        std::vector<float> splits(safeCount, safeFar);
        const float ratio = safeFar / safeNear;
        for (std::uint32_t partition = 1u; partition <= safeCount; ++partition)
        {
            const float fraction = static_cast<float>(partition) / static_cast<float>(safeCount);
            const float logarithmic = safeNear * std::pow(ratio, fraction);
            const float uniform = safeNear + (safeFar - safeNear) * fraction;
            splits[partition - 1u] = partition == safeCount
                                         ? safeFar
                                         : std::lerp(uniform, logarithmic, lambda);
        }
        return splits;
    }

    std::array<glm::vec3, 8> FrustumPartition::BuildSliceCorners(
        const RenderView &view,
        float sliceNearDistance,
        float sliceFarDistance)
    {
        const float viewNear = std::max(view.nearClipMeters, 0.001f);
        const float viewFar = std::max(view.farClipMeters, viewNear + 0.001f);
        const float sliceNear = std::clamp(sliceNearDistance, viewNear, viewFar);
        const float sliceFar = std::clamp(sliceFarDistance, sliceNear, viewFar);
        const float inverseDepthRange = 1.f / (viewFar - viewNear);
        const float nearFactor = (sliceNear - viewNear) * inverseDepthRange;
        const float farFactor = (sliceFar - viewNear) * inverseDepthRange;

        constexpr std::array<glm::vec2, 4> ClipCorners{
            glm::vec2(-1.f, -1.f),
            glm::vec2(1.f, -1.f),
            glm::vec2(1.f, 1.f),
            glm::vec2(-1.f, 1.f)};
        std::array<glm::vec3, 8> corners{};
        for (std::size_t corner = 0u; corner < ClipCorners.size(); ++corner)
        {
            glm::vec4 worldNear = view.inverseViewProjection * glm::vec4(ClipCorners[corner], -1.f, 1.f);
            glm::vec4 worldFar = view.inverseViewProjection * glm::vec4(ClipCorners[corner], 1.f, 1.f);
            worldNear /= std::max(std::abs(worldNear.w), 0.000001f) * (worldNear.w < 0.f ? -1.f : 1.f);
            worldFar /= std::max(std::abs(worldFar.w), 0.000001f) * (worldFar.w < 0.f ? -1.f : 1.f);

            const glm::vec3 nearCorner = glm::vec3(worldNear);
            const glm::vec3 farCorner = glm::vec3(worldFar);
            corners[corner] = glm::mix(nearCorner, farCorner, nearFactor);
            corners[corner + 4u] = glm::mix(nearCorner, farCorner, farFactor);
        }
        return corners;
    }
}