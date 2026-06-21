#include "ClusteredLightGrid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>

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

    void ClusteredLightGrid::Build(FrameData &frameData)
    {
        const std::uint32_t width = std::max(frameData.view.viewport.width, 1u);
        const std::uint32_t height = std::max(frameData.view.viewport.height, 1u);
        const float nearDistance = std::max(frameData.view.nearClipMeters, 0.001f);
        const float farDistance = std::max(frameData.view.farClipMeters, nearDistance + 0.001f);
        const std::uint32_t tileSize = ClusteredLightGridDetail::BuildTileSize(width, height);
        const bool topologyChanged =
            width != m_CachedWidth ||
            height != m_CachedHeight ||
            tileSize != m_CachedTileSize ||
            nearDistance != m_CachedNearDistance ||
            farDistance != m_CachedFarDistance;
        if (topologyChanged)
        {
            m_CachedWidth = width;
            m_CachedHeight = height;
            m_CachedTileSize = tileSize;
            m_CachedNearDistance = nearDistance;
            m_CachedFarDistance = farDistance;
            m_CachedClusterCountX = ClusteredLightGridDetail::DivideRoundUp(width, tileSize);
            m_CachedClusterCountY = ClusteredLightGridDetail::DivideRoundUp(height, tileSize);
            m_DepthPartition = FrustumPartition::BuildLogarithmicDepthPartition(
                nearDistance,
                farDistance,
                ClusteredLightGridDetail::SliceCount);
        }

        const std::uint32_t clusterCount =
            m_CachedClusterCountX * m_CachedClusterCountY * ClusteredLightGridDetail::SliceCount;
        frameData.clusterGrid.dimensions = glm::uvec4(
            m_CachedClusterCountX,
            m_CachedClusterCountY,
            ClusteredLightGridDetail::SliceCount,
            tileSize);
        frameData.clusterGrid.depthParams = glm::vec4(
            nearDistance,
            farDistance,
            m_DepthPartition.sliceScale,
            m_DepthPartition.sliceBias);
        frameData.clusterGrid.counts = glm::uvec4(clusterCount, 0u, 0u, 0u);
        frameData.clusterEntries.resize(clusterCount);
        frameData.clusterLightIndices.clear();
        frameData.stats.clusterCount = clusterCount;
        frameData.stats.clusterLightReferences = 0u;
        frameData.stats.maxLightsPerCluster = 0u;
        frameData.stats.localLightCount = 0u;
        frameData.stats.clusterOverflowedLightReferences = 0u;
        if (clusterCount == 0u)
        {
            return;
        }

        m_ClusterCounts.assign(clusterCount, 0u);
        m_ClusterOffsets.resize(clusterCount + 1u);
        m_ClusterFillCursors.resize(clusterCount);
        m_LightBounds.clear();
        m_LightBounds.reserve(frameData.lights.size());

        const glm::mat4 &view = frameData.view.view;
        const glm::mat4 &projection = frameData.view.projection;
        for (std::uint32_t lightIndex = 0u; lightIndex < static_cast<std::uint32_t>(frameData.lights.size()); ++lightIndex)
        {
            const LightData &light = frameData.lights[lightIndex];
            if (!ClusteredLightGridDetail::IsLocalLight(light))
            {
                continue;
            }
            ++frameData.stats.localLightCount;

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
            LightClusterBounds bounds{};
            bounds.lightIndex = lightIndex;
            bounds.minX = std::min(static_cast<std::uint32_t>(pixelMin.x) / tileSize, m_CachedClusterCountX - 1u);
            bounds.maxX = std::min(static_cast<std::uint32_t>(pixelMax.x) / tileSize, m_CachedClusterCountX - 1u);
            bounds.minY = std::min(static_cast<std::uint32_t>(pixelMin.y) / tileSize, m_CachedClusterCountY - 1u);
            bounds.maxY = std::min(static_cast<std::uint32_t>(pixelMax.y) / tileSize, m_CachedClusterCountY - 1u);
            bounds.minZ = m_DepthPartition.GetSlice(minDepth);
            bounds.maxZ = m_DepthPartition.GetSlice(maxDepth);
            m_LightBounds.push_back(bounds);

            for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
            {
                for (std::uint32_t y = bounds.minY; y <= bounds.maxY; ++y)
                {
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                    {
                        const std::uint32_t clusterIndex =
                            x + m_CachedClusterCountX * (y + m_CachedClusterCountY * z);
                        if (m_ClusterCounts[clusterIndex] < ClusteredLightGridDetail::MaxLightsPerCluster)
                        {
                            ++m_ClusterCounts[clusterIndex];
                        }
                        else
                        {
                            ++frameData.stats.clusterOverflowedLightReferences;
                        }
                    }
                }
            }
        }

        if (m_LightBounds.empty())
        {
            return;
        }

        m_ClusterOffsets[0] = 0u;
        std::uint32_t maxLightsInCluster = 0u;
        for (std::uint32_t clusterIndex = 0u; clusterIndex < clusterCount; ++clusterIndex)
        {
            m_ClusterOffsets[clusterIndex + 1u] =
                m_ClusterOffsets[clusterIndex] + m_ClusterCounts[clusterIndex];
            m_ClusterFillCursors[clusterIndex] = m_ClusterOffsets[clusterIndex];
            ClusterEntryGPU &entry = frameData.clusterEntries[clusterIndex];
            entry.offset = m_ClusterOffsets[clusterIndex];
            entry.count = m_ClusterCounts[clusterIndex];
            maxLightsInCluster = std::max(maxLightsInCluster, entry.count);
        }

        frameData.clusterLightIndices.resize(m_ClusterOffsets[clusterCount]);
        for (const LightClusterBounds &bounds : m_LightBounds)
        {
            for (std::uint32_t z = bounds.minZ; z <= bounds.maxZ; ++z)
            {
                for (std::uint32_t y = bounds.minY; y <= bounds.maxY; ++y)
                {
                    for (std::uint32_t x = bounds.minX; x <= bounds.maxX; ++x)
                    {
                        const std::uint32_t clusterIndex =
                            x + m_CachedClusterCountX * (y + m_CachedClusterCountY * z);
                        std::uint32_t &cursor = m_ClusterFillCursors[clusterIndex];
                        const std::uint32_t end = m_ClusterOffsets[clusterIndex + 1u];
                        if (cursor < end)
                        {
                            frameData.clusterLightIndices[cursor++] = bounds.lightIndex;
                        }
                    }
                }
            }
        }

        frameData.clusterGrid.counts.y = static_cast<std::uint32_t>(frameData.clusterLightIndices.size());
        frameData.clusterGrid.counts.z = maxLightsInCluster;
        frameData.stats.clusterLightReferences = frameData.clusterGrid.counts.y;
        frameData.stats.maxLightsPerCluster = maxLightsInCluster;
    }
}
