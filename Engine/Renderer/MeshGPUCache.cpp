#include "MeshGPUCache.hpp"

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
        std::uint64_t BuildRenderPrimitiveId(std::uint64_t meshKey, std::size_t primitiveIndex)
        {
            std::uint64_t seed = meshKey;
            const std::uint64_t value = static_cast<std::uint64_t>(primitiveIndex);
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
            return seed != 0 ? seed : 1ull;
        }

        template <typename T>
        RHI::RHIBufferDesc StaticBufferDesc(const std::vector<T> &data, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = static_cast<std::uint32_t>(data.size() * sizeof(T));
            desc.usage = usage;
            desc.dynamic = false;
            desc.initialData = data.data();
            return desc;
        }
    }

    MeshGPUPrimitive *MeshGPUCache::GetOrCreate(
        RHI::RHIDevice *device,
        AssetManager *assetManager,
        const RenderDrawItem &item,
        FrameStatistics *stats)
    {
        MeshGPUResource *resource = GetOrCreateMeshResource(device, assetManager, item, stats);
        if (resource == nullptr || item.submission == nullptr || item.submission->primitiveIndex >= resource->primitives.size())
        {
            return nullptr;
        }

        return &resource->primitives[item.submission->primitiveIndex];
    }

    MeshGPUResource *MeshGPUCache::GetOrCreateMeshResource(
        RHI::RHIDevice *device,
        AssetManager *assetManager,
        const RenderDrawItem &item,
        FrameStatistics *stats)
    {
        if (assetManager == nullptr || device == nullptr || item.submission == nullptr)
        {
            return nullptr;
        }

        const auto cached = m_MeshCache.find(item.meshKey);
        if (cached != m_MeshCache.end())
        {
            return &cached->second;
        }

        const std::string meshResourcePath = BuildMeshResourcePath(item);
        const std::shared_ptr<Mesh> mesh = assetManager->GetByPath<Mesh>(meshResourcePath);
        if (mesh == nullptr || item.submission->primitiveIndex >= mesh->primitives.size())
        {
            if (m_MissingMeshWarnings.insert(item.meshKey).second)
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
            if (m_MissingMeshWarnings.insert(item.meshKey).second)
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
            gpuPrimitive.renderPrimitiveId = MeshGPUCacheDetail::BuildRenderPrimitiveId(item.meshKey, primitiveIndex);

            vertices.insert(vertices.end(), primitive.vertices.begin(), primitive.vertices.end());
            indices.insert(indices.end(), primitive.indices.begin(), primitive.indices.end());
        }

        gpuResource.vertexBuffer = device->CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(vertices, RHI::BufferUsage::Vertex));
        gpuResource.indexBuffer = device->CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(indices, RHI::BufferUsage::Index));

        if (gpuResource.vertexBuffer == nullptr || gpuResource.indexBuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Mesh GPU cache failed to upload '{}'.", meshResourcePath);
            return nullptr;
        }

        for (MeshGPUPrimitive &primitive : gpuResource.primitives)
        {
            if (primitive.indexCount == 0)
            {
                continue;
            }
            primitive.vertexBindings[0] = RHI::RHIVertexBufferBinding{0u, gpuResource.vertexBuffer.get(), 0u};
            primitive.indexBinding = RHI::RHIIndexBufferBinding{gpuResource.indexBuffer.get(), 0u};
        }

        if (stats != nullptr)
        {
            const std::uint64_t vertexBytes = static_cast<std::uint64_t>(vertices.size() * sizeof(MeshVertex));
            const std::uint64_t indexBytes = static_cast<std::uint64_t>(indices.size() * sizeof(std::uint32_t));
            ++stats->meshUploads;
            stats->meshPrimitiveUploads += static_cast<std::uint32_t>(mesh->primitives.size());
            stats->meshUploadBytes += vertexBytes + indexBytes;
        }

        PHYSARA_CORE_INFO("Mesh GPU cache uploaded '{}': primitives={}, vertices={}, indices={}.",
                          meshResourcePath,
                          mesh->primitives.size(),
                          vertices.size(),
                          indices.size());
        auto [inserted, _] = m_MeshCache.emplace(item.meshKey, std::move(gpuResource));
        return &inserted->second;
    }

    void MeshGPUCache::Reset()
    {
        m_MeshCache.clear();
        m_MissingMeshWarnings.clear();
    }

    std::string MeshGPUCache::BuildMeshResourcePath(const RenderDrawItem &item)
    {
        if (item.submission == nullptr)
        {
            return {};
        }

        return item.submission->meshPath + "#mesh/" + std::to_string(item.submission->meshIndex);
    }

    std::string MeshGPUCache::BuildMeshPrimitiveDebugName(const RenderDrawItem &item)
    {
        if (item.submission == nullptr)
        {
            return {};
        }

        return BuildMeshResourcePath(item) + "#primitive/" + std::to_string(item.submission->primitiveIndex);
    }
}