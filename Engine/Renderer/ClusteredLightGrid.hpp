#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrustumPartition.hpp>
#include <Engine/Renderer/GPUContracts.hpp>

namespace Physara::Engine
{
    struct FrameData;

    class ClusteredLightGrid final
    {
    public:
        void Build(FrameData &frameData);

    private:
        struct LightClusterBounds
        {
            std::uint32_t lightIndex{0};
            std::uint32_t minX{0};
            std::uint32_t maxX{0};
            std::uint32_t minY{0};
            std::uint32_t maxY{0};
            std::uint32_t minZ{0};
            std::uint32_t maxZ{0};
        };

        std::vector<std::uint32_t> m_ClusterCounts{};
        std::vector<std::uint32_t> m_ClusterOffsets{};
        std::vector<std::uint32_t> m_ClusterFillCursors{};
        std::vector<LightClusterBounds> m_LightBounds{};
        LogarithmicDepthPartition m_DepthPartition{};
        std::uint32_t m_CachedWidth{0};
        std::uint32_t m_CachedHeight{0};
        std::uint32_t m_CachedTileSize{0};
        std::uint32_t m_CachedClusterCountX{0};
        std::uint32_t m_CachedClusterCountY{0};
        float m_CachedNearDistance{0.f};
        float m_CachedFarDistance{0.f};
    };
}
