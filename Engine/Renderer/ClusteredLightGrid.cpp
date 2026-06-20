#include "ClusteredLightGrid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrustumPartition.hpp>

namespace Physara::Engine
{
    namespace ClusteredLightGridDetail
    {
        constexpr std::uint32_t SliceCount = 16u;
        constexpr std::uint32_t MaxClusterCount = 8192u;
        constexpr std::uint32_t MaxLightsPerCluster = 64u;
        constexpr std::uint32_t TileAlignment = 8u;

        std::uint32_t DivideRoundUp(std::uint32_t value, std::uint32_t divisor)
        {
            return divisor == 0u ? 0u : (value + divisor - 1u) / divisor;
        }

        std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment)
        {
            return alignment == 0u ? value : DivideRoundUp(value, alignment) * alignment;
        }

        std::uint32_t BuildTileSize(std::uint32_t width, std::uint32_t height)
        {
            const float planeBudget = static_cast<float>(MaxClusterCount / SliceCount);
            const float pixelCount = static_cast<float>(std::max(width, 1u)) * static_cast<float>(std::max(height, 1u));
            std::uint32_t tileSize = AlignUp(
                static_cast<std::uint32_t>(std::ceil(std::sqrt(pixelCount / planeBudget))),
                TileAlignment);
            tileSize = std::max(tileSize, TileAlignment);
            while (DivideRoundUp(width, tileSize) * DivideRoundUp(height, tileSize) * SliceCount > MaxClusterCount)
            {
                tileSize += TileAlignment;
            }
            return tileSize;
        }

        bool IsLocalLight(const LightData &light)
        {
            const std::uint32_t type = static_cast<std::uint32_t>(light.directionType.w + 0.5f);
            return type == GPUValue(LightTypeGPU::Point) || type == GPUValue(LightTypeGPU::Spot);
        }
    }

    void ClusteredLightGrid::Build(FrameData &frameData) const
    {
        const std::uint32_t width = std::max(frameData.view.viewport.width, 1u);
        const std::uint32_t height = std::max(frameData.view.viewport.height, 1u);
        const float nearDistance = std::max(frameData.view.nearClipMeters, 0.001f);
        const float farDistance = std::max(frameData.view.farClipMeters, nearDistance + 0.001f);
        const std::uint32_t tileSize = ClusteredLightGridDetail::BuildTileSize(width, height);
        const std::uint32_t clusterCountX = ClusteredLightGridDetail::DivideRoundUp(width, tileSize);
        const std::uint32_t clusterCountY = ClusteredLightGridDetail::DivideRoundUp(height, tileSize);
        const std::uint32_t clusterCount = clusterCountX * clusterCountY * ClusteredLightGridDetail::SliceCount;
        const LogarithmicDepthPartition depthPartition = FrustumPartition::BuildLogarithmicDepthPartition(
            nearDistance,
            farDistance,
            ClusteredLightGridDetail::SliceCount);

        frameData.clusterGrid.dimensions = glm::uvec4(
            clusterCountX,
            clusterCountY,
            ClusteredLightGridDetail::SliceCount,
            tileSize);
        frameData.clusterGrid.depthParams = glm::vec4(
            nearDistance,
            farDistance,
            depthPartition.sliceScale,
            depthPartition.sliceBias);
        frameData.clusterGrid.counts = glm::uvec4(clusterCount, 0u, 0u, 0u);
        frameData.clusterEntries.assign(clusterCount, {});
        frameData.clusterLightIndices.clear();
        if (clusterCount == 0u)
        {
            return;
        }

        std::vector<std::vector<std::uint32_t>> clusterLists(clusterCount);
        const glm::mat4 &view = frameData.view.view;
        const glm::mat4 &projection = frameData.view.projection;
        for (std::uint32_t lightIndex = 0u; lightIndex < static_cast<std::uint32_t>(frameData.lights.size()); ++lightIndex)
        {
            const LightData &light = frameData.lights[lightIndex];
            if (!ClusteredLightGridDetail::IsLocalLight(light))
            {
                continue;
            }

            const glm::vec3 viewPosition = glm::vec3(view * glm::vec4(glm::vec3(light.positionRange), 1.f));
            const float radius = std::max(light.positionRange.w, 0.001f);
            const float centerDepth = -viewPosition.z;
            const float minDepth = std::max(centerDepth - radius, nearDistance);
            const float maxDepth = std::min(centerDepth + radius, farDistance);
            if (maxDepth < nearDistance || minDepth > farDistance || centerDepth + radius <= 0.f)
            {
                continue;
            }

            const float projectionDepth = std::max(centerDepth - radius, nearDistance);
            const glm::vec4 clipCenter = projection * glm::vec4(viewPosition, 1.f);
            const float inverseW = 1.f / std::max(std::abs(clipCenter.w), 0.000001f);
            const glm::vec2 ndcCenter = glm::vec2(clipCenter) * inverseW;
            const glm::vec2 ndcRadius(
                std::abs(projection[0][0]) * radius / projectionDepth,
                std::abs(projection[1][1]) * radius / projectionDepth);
            const glm::vec2 pixelMin = glm::clamp(
                (ndcCenter - ndcRadius) * 0.5f + 0.5f,
                glm::vec2(0.f),
                glm::vec2(1.f)) * glm::vec2(width, height);
            const glm::vec2 pixelMax = glm::clamp(
                (ndcCenter + ndcRadius) * 0.5f + 0.5f,
                glm::vec2(0.f),
                glm::vec2(1.f)) * glm::vec2(width, height);
            const std::uint32_t minX = std::min(static_cast<std::uint32_t>(pixelMin.x) / tileSize, clusterCountX - 1u);
            const std::uint32_t maxX = std::min(static_cast<std::uint32_t>(pixelMax.x) / tileSize, clusterCountX - 1u);
            const std::uint32_t minY = std::min(static_cast<std::uint32_t>(pixelMin.y) / tileSize, clusterCountY - 1u);
            const std::uint32_t maxY = std::min(static_cast<std::uint32_t>(pixelMax.y) / tileSize, clusterCountY - 1u);
            const std::uint32_t minZ = depthPartition.GetSlice(minDepth);
            const std::uint32_t maxZ = depthPartition.GetSlice(maxDepth);

            for (std::uint32_t z = minZ; z <= maxZ; ++z)
            {
                for (std::uint32_t y = minY; y <= maxY; ++y)
                {
                    for (std::uint32_t x = minX; x <= maxX; ++x)
                    {
                        const std::uint32_t clusterIndex = x + clusterCountX * (y + clusterCountY * z);
                        std::vector<std::uint32_t> &indices = clusterLists[clusterIndex];
                        if (indices.size() < ClusteredLightGridDetail::MaxLightsPerCluster)
                        {
                            indices.push_back(lightIndex);
                        }
                    }
                }
            }
        }

        std::uint32_t maxLightsInCluster = 0u;
        for (std::uint32_t clusterIndex = 0u; clusterIndex < clusterCount; ++clusterIndex)
        {
            const std::vector<std::uint32_t> &indices = clusterLists[clusterIndex];
            ClusterEntryGPU &entry = frameData.clusterEntries[clusterIndex];
            entry.offset = static_cast<std::uint32_t>(frameData.clusterLightIndices.size());
            entry.count = static_cast<std::uint32_t>(indices.size());
            maxLightsInCluster = std::max(maxLightsInCluster, entry.count);
            frameData.clusterLightIndices.insert(frameData.clusterLightIndices.end(), indices.begin(), indices.end());
        }
        frameData.clusterGrid.counts.y = static_cast<std::uint32_t>(frameData.clusterLightIndices.size());
        frameData.clusterGrid.counts.z = maxLightsInCluster;
        frameData.stats.clusterCount = clusterCount;
        frameData.stats.clusterLightReferences = frameData.clusterGrid.counts.y;
        frameData.stats.maxLightsPerCluster = maxLightsInCluster;
    }

}