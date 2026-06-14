#include "MeshGPUCache.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>

namespace Physara::Engine
{
    namespace MeshGPUCacheDetail
    {
        constexpr std::uint32_t GeometryPageVertexBytes = 64u * 1024u * 1024u;
        constexpr std::uint32_t GeometryPageIndexBytes = 32u * 1024u * 1024u;

        std::string BuildMeshResourcePath(const RenderMeshSubmission *submission)
        {
            if (submission == nullptr)
            {
                return {};
            }

            return std::string(submission->meshPath) + "#mesh/" + std::to_string(submission->meshIndex);
        }

        std::string BuildMeshPrimitiveDebugName(const RenderMeshSubmission *submission)
        {
            if (submission == nullptr)
            {
                return {};
            }

            return BuildMeshResourcePath(submission) + "#primitive/" + std::to_string(submission->primitiveIndex);
        }

        std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment)
        {
            return ((value + alignment - 1u) / alignment) * alignment;
        }

        RHI::RHIBufferDesc StaticBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = size;
            desc.usage = usage;
            desc.dynamic = false;
            return desc;
        }
    }

    MeshGPUPrimitive *MeshGPUCache::GetOrCreate(
        RHI::RHIDevice *device,
        AssetManager *assetManager,
        const RenderDrawItem &item,
        FrameStatistics *stats)
    {
        MeshGPUResource *resource = GetOrCreateMeshResource(device, assetManager, item.submission, item.meshKey, stats);
        if (resource == nullptr || item.submission == nullptr || item.submission->primitiveIndex >= resource->primitives.size())
        {
            return nullptr;
        }

        return &resource->primitives[item.submission->primitiveIndex];
    }

    MeshGPUPrimitive *MeshGPUCache::GetOrCreate(
        RHI::RHIDevice *device,
        AssetManager *assetManager,
        const RenderCommand &command,
        FrameStatistics *stats)
    {
        MeshGPUResource *resource = GetOrCreateMeshResource(device, assetManager, command.submission, command.meshKey, stats);
        if (resource == nullptr || command.submission == nullptr || command.submission->primitiveIndex >= resource->primitives.size())
        {
            return nullptr;
        }

        return &resource->primitives[command.submission->primitiveIndex];
    }

    MeshGPUResource *MeshGPUCache::GetOrCreateMeshResource(
        RHI::RHIDevice *device,
        AssetManager *assetManager,
        const RenderMeshSubmission *submission,
        std::uint64_t meshKey,
        FrameStatistics *stats)
    {
        if (assetManager == nullptr || device == nullptr || submission == nullptr)
        {
            return nullptr;
        }

        const auto cached = m_MeshCache.find(meshKey);
        if (cached != m_MeshCache.end())
        {
            return &cached->second;
        }

        const std::string meshResourcePath = MeshGPUCacheDetail::BuildMeshResourcePath(submission);
        const std::shared_ptr<Mesh> mesh = assetManager->GetByPath<Mesh>(meshResourcePath);
        if (mesh == nullptr || submission->primitiveIndex >= mesh->primitives.size())
        {
            if (m_MissingMeshWarnings.insert(meshKey).second)
            {
                PHYSARA_CORE_WARN("Mesh GPU cache skipped '{}': resource not found or primitive index out of range. normalized='{}'.",
                                  meshResourcePath,
                                  assetManager->NormalizePath(meshResourcePath));
            }
            return nullptr;
        }

        std::size_t totalVertexCount = 0;
        std::size_t totalIndexCount = 0;
        for (const MeshPrimitive &primitive : mesh->primitives)
        {
            if (!primitive.HasGeometry())
            {
                continue;
            }
            totalVertexCount += primitive.vertices.size();
            totalIndexCount += primitive.indices.size();
        }

        if (totalVertexCount == 0 || totalIndexCount == 0)
        {
            if (m_MissingMeshWarnings.insert(meshKey).second)
            {
                PHYSARA_CORE_WARN("Mesh GPU cache skipped '{}': mesh has no decoded geometry.", meshResourcePath);
            }
            return nullptr;
        }

        if (totalVertexCount > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            PHYSARA_CORE_ERROR("Mesh GPU cache skipped '{}': vertex count exceeds base vertex range.", meshResourcePath);
            return nullptr;
        }
        if (totalIndexCount > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            PHYSARA_CORE_ERROR("Mesh GPU cache skipped '{}': index count exceeds RHI draw range.", meshResourcePath);
            return nullptr;
        }

        std::vector<MeshVertex> vertices;
        std::vector<std::uint32_t> indices;
        vertices.reserve(totalVertexCount);
        indices.reserve(totalIndexCount);

        MeshGPUResource gpuResource{};
        gpuResource.primitives.resize(mesh->primitives.size());

        for (std::size_t primitiveIndex = 0; primitiveIndex < mesh->primitives.size(); ++primitiveIndex)
        {
            const MeshPrimitive &primitive = mesh->primitives[primitiveIndex];
            if (!primitive.HasGeometry())
            {
                continue;
            }

            MeshGPUPrimitive &gpuPrimitive = gpuResource.primitives[primitiveIndex];
            gpuPrimitive.firstIndex = static_cast<std::uint32_t>(indices.size());
            gpuPrimitive.vertexOffset = static_cast<std::int32_t>(vertices.size());
            gpuPrimitive.indexCount = static_cast<std::uint32_t>(primitive.indices.size());

            vertices.insert(vertices.end(), primitive.vertices.begin(), primitive.vertices.end());
            indices.insert(indices.end(), primitive.indices.begin(), primitive.indices.end());
        }

        const std::uint32_t vertexBytes = static_cast<std::uint32_t>(vertices.size() * sizeof(MeshVertex));
        const std::uint32_t indexBytes = static_cast<std::uint32_t>(indices.size() * sizeof(std::uint32_t));
        GeometryAllocation allocation = AllocateGeometry(*device, vertexBytes, indexBytes);
        if (allocation.page == nullptr ||
            allocation.page->vertexBuffer == nullptr ||
            allocation.page->indexBuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Mesh GPU cache failed to upload '{}'.", meshResourcePath);
            return nullptr;
        }

        allocation.page->vertexBuffer->UploadData(vertices.data(), vertexBytes, allocation.vertexByteOffset);
        allocation.page->indexBuffer->UploadData(indices.data(), indexBytes, allocation.indexByteOffset);

        const std::uint32_t vertexBase = allocation.vertexByteOffset / static_cast<std::uint32_t>(sizeof(MeshVertex));
        const std::uint32_t indexBase = allocation.indexByteOffset / static_cast<std::uint32_t>(sizeof(std::uint32_t));
        for (MeshGPUPrimitive &primitive : gpuResource.primitives)
        {
            if (primitive.indexCount == 0)
            {
                continue;
            }
            primitive.firstIndex += indexBase;
            primitive.vertexOffset += static_cast<std::int32_t>(vertexBase);
            primitive.geometryBindingId = allocation.page->bindingId;
            primitive.vertexBindings[0] = RHI::RHIVertexBufferBinding{0u, allocation.page->vertexBuffer.get(), 0u};
            primitive.indexBinding = RHI::RHIIndexBufferBinding{allocation.page->indexBuffer.get(), 0u};
        }

        if (stats != nullptr)
        {
            ++stats->meshUploads;
            stats->meshPrimitiveUploads += static_cast<std::uint32_t>(mesh->primitives.size());
            stats->meshUploadBytes += vertexBytes + indexBytes;
        }

        PHYSARA_CORE_INFO("Mesh GPU cache uploaded '{}': primitives={}, vertices={}, indices={}.",
                          meshResourcePath,
                          mesh->primitives.size(),
                          vertices.size(),
                          indices.size());
        auto [inserted, _] = m_MeshCache.emplace(meshKey, std::move(gpuResource));
        return &inserted->second;
    }

    MeshGPUCache::GeometryAllocation MeshGPUCache::AllocateGeometry(
        RHI::RHIDevice &device,
        std::uint32_t vertexBytes,
        std::uint32_t indexBytes)
    {
        const std::uint32_t alignedVertexBytes = MeshGPUCacheDetail::AlignUp(vertexBytes, static_cast<std::uint32_t>(sizeof(MeshVertex)));
        const std::uint32_t alignedIndexBytes = MeshGPUCacheDetail::AlignUp(indexBytes, static_cast<std::uint32_t>(sizeof(std::uint32_t)));

        for (std::unique_ptr<GeometryPage> &page : m_GeometryPages)
        {
            if (page == nullptr)
            {
                continue;
            }

            const std::uint32_t vertexOffset = MeshGPUCacheDetail::AlignUp(page->vertexBytesUsed, static_cast<std::uint32_t>(sizeof(MeshVertex)));
            const std::uint32_t indexOffset = MeshGPUCacheDetail::AlignUp(page->indexBytesUsed, static_cast<std::uint32_t>(sizeof(std::uint32_t)));
            if (vertexOffset <= page->vertexCapacityBytes &&
                indexOffset <= page->indexCapacityBytes &&
                alignedVertexBytes <= page->vertexCapacityBytes - vertexOffset &&
                alignedIndexBytes <= page->indexCapacityBytes - indexOffset)
            {
                page->vertexBytesUsed = vertexOffset + alignedVertexBytes;
                page->indexBytesUsed = indexOffset + alignedIndexBytes;
                return GeometryAllocation{page.get(), vertexOffset, indexOffset};
            }
        }

        GeometryPage *page = CreateGeometryPage(device, vertexBytes, indexBytes);
        if (page == nullptr)
        {
            return {};
        }

        page->vertexBytesUsed = alignedVertexBytes;
        page->indexBytesUsed = alignedIndexBytes;
        return GeometryAllocation{page, 0u, 0u};
    }

    MeshGPUCache::GeometryPage *MeshGPUCache::CreateGeometryPage(
        RHI::RHIDevice &device,
        std::uint32_t vertexBytes,
        std::uint32_t indexBytes)
    {
        auto page = std::make_unique<GeometryPage>();
        page->vertexCapacityBytes = std::max(vertexBytes, MeshGPUCacheDetail::GeometryPageVertexBytes);
        page->indexCapacityBytes = std::max(indexBytes, MeshGPUCacheDetail::GeometryPageIndexBytes);
        page->bindingId = m_NextGeometryBindingId++;
        page->vertexBuffer = device.CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(page->vertexCapacityBytes, RHI::BufferUsage::Vertex));
        page->indexBuffer = device.CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(page->indexCapacityBytes, RHI::BufferUsage::Index));
        if (page->vertexBuffer == nullptr || page->indexBuffer == nullptr)
        {
            return nullptr;
        }

        GeometryPage *rawPage = page.get();
        m_GeometryPages.push_back(std::move(page));
        return rawPage;
    }

    void MeshGPUCache::Reset()
    {
        m_MeshCache.clear();
        m_MissingMeshWarnings.clear();
        m_GeometryPages.clear();
        m_NextGeometryBindingId = 1;
    }

    std::string MeshGPUCache::BuildMeshResourcePath(const RenderDrawItem &item)
    {
        return MeshGPUCacheDetail::BuildMeshResourcePath(item.submission);
    }

    std::string MeshGPUCache::BuildMeshResourcePath(const RenderCommand &command)
    {
        return MeshGPUCacheDetail::BuildMeshResourcePath(command.submission);
    }

    std::string MeshGPUCache::BuildMeshPrimitiveDebugName(const RenderDrawItem &item)
    {
        return MeshGPUCacheDetail::BuildMeshPrimitiveDebugName(item.submission);
    }

    std::string MeshGPUCache::BuildMeshPrimitiveDebugName(const RenderCommand &command)
    {
        return MeshGPUCacheDetail::BuildMeshPrimitiveDebugName(command.submission);
    }
}
