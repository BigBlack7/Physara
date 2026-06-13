#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Engine/RHI/Descriptors/RHIRenderPrimitive.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    struct FrameStatistics;
    struct RenderCommand;
    struct RenderDrawItem;
    struct RenderMeshSubmission;

    struct MeshGPUPrimitive
    {
        std::array<RHI::RHIVertexBufferBinding, 1> vertexBindings{};
        RHI::RHIIndexBufferBinding indexBinding{};
        std::uint32_t indexCount{0};
        std::uint32_t firstIndex{0};
        std::int32_t vertexOffset{0};
        std::uint64_t renderPrimitiveId{0};

        [[nodiscard]] RHI::RHIRenderPrimitive AsRHIRenderPrimitive() const
        {
            return RHI::RHIRenderPrimitive{
                renderPrimitiveId,
                std::span<const RHI::RHIVertexBufferBinding>{vertexBindings.data(), vertexBindings.size()},
                indexBinding};
        }
    };

    struct MeshGPUResource
    {
        std::unique_ptr<RHI::RHIBuffer> vertexBuffer{};
        std::unique_ptr<RHI::RHIBuffer> indexBuffer{};
        std::vector<MeshGPUPrimitive> primitives{};
    };

    class MeshGPUCache final
    {
    public:
        [[nodiscard]] MeshGPUPrimitive *GetOrCreate(
            RHI::RHIDevice *device,
            AssetManager *assetManager,
            const RenderDrawItem &item,
            FrameStatistics *stats);
        [[nodiscard]] MeshGPUPrimitive *GetOrCreate(
            RHI::RHIDevice *device,
            AssetManager *assetManager,
            const RenderCommand &command,
            FrameStatistics *stats);

        void Reset();

        [[nodiscard]] static std::string BuildMeshResourcePath(const RenderDrawItem &item);
        [[nodiscard]] static std::string BuildMeshResourcePath(const RenderCommand &command);
        [[nodiscard]] static std::string BuildMeshPrimitiveDebugName(const RenderDrawItem &item);
        [[nodiscard]] static std::string BuildMeshPrimitiveDebugName(const RenderCommand &command);

    private:
        [[nodiscard]] MeshGPUResource *GetOrCreateMeshResource(
            RHI::RHIDevice *device,
            AssetManager *assetManager,
            const RenderMeshSubmission *submission,
            std::uint64_t meshKey,
            FrameStatistics *stats);

    private:
        std::unordered_map<std::uint64_t, MeshGPUResource> m_MeshCache{};
        std::unordered_set<std::uint64_t> m_MissingMeshWarnings{};
    };
}