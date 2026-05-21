#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    struct FrameStatistics;
    struct RenderDrawItem;

    struct MeshGPUPrimitive
    {
        std::unique_ptr<RHI::RHIBuffer> vertexBuffer{};
        std::unique_ptr<RHI::RHIBuffer> indexBuffer{};
        std::uint32_t indexCount{0};
    };

    class MeshGPUCache final
    {
    public:
        [[nodiscard]] MeshGPUPrimitive *GetOrCreate(
            RHI::RHIDevice *device,
            AssetManager *assetManager,
            const RenderDrawItem &item,
            FrameStatistics *stats);

        void Reset();

        [[nodiscard]] static std::string BuildMeshResourcePath(const RenderDrawItem &item);
        [[nodiscard]] static std::string BuildMeshPrimitiveDebugName(const RenderDrawItem &item);

    private:
        std::unordered_map<std::uint64_t, MeshGPUPrimitive> m_MeshCache{};
        std::unordered_set<std::uint64_t> m_MissingMeshWarnings{};
    };
}