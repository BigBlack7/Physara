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
        std::uint64_t geometryBindingId{0};

        [[nodiscard]] RHI::RHIRenderPrimitive AsRHIRenderPrimitive() const
        {
            return RHI::RHIRenderPrimitive{
                geometryBindingId,
                std::span<const RHI::RHIVertexBufferBinding>{vertexBindings.data(), vertexBindings.size()},
                indexBinding};
        }
    };

    struct MeshGPUResource
    {
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
        struct GeometryPage
        {
            std::unique_ptr<RHI::RHIBuffer> vertexBuffer{};
            std::unique_ptr<RHI::RHIBuffer> indexBuffer{};
            std::uint32_t vertexCapacityBytes{0};
            std::uint32_t indexCapacityBytes{0};
            std::uint32_t vertexBytesUsed{0};
            std::uint32_t indexBytesUsed{0};
            std::uint64_t bindingId{0};
        };

        struct GeometryAllocation
        {
            GeometryPage *page{nullptr};
            std::uint32_t vertexByteOffset{0};
            std::uint32_t indexByteOffset{0};
        };

        [[nodiscard]] MeshGPUResource *GetOrCreateMeshResource(
            RHI::RHIDevice *device,
            AssetManager *assetManager,
            const RenderMeshSubmission *submission,
            std::uint64_t meshKey,
            FrameStatistics *stats);
        [[nodiscard]] GeometryAllocation AllocateGeometry(
            RHI::RHIDevice &device,
            std::uint32_t vertexBytes,
            std::uint32_t indexBytes);
        [[nodiscard]] GeometryPage *CreateGeometryPage(
            RHI::RHIDevice &device,
            std::uint32_t vertexBytes,
            std::uint32_t indexBytes);

    private:
        std::unordered_map<std::uint64_t, MeshGPUResource> m_MeshCache{};
        std::unordered_set<std::uint64_t> m_MissingMeshWarnings{};
        std::vector<std::unique_ptr<GeometryPage>> m_GeometryPages{};
        std::uint64_t m_NextGeometryBindingId{1};
    };
}
