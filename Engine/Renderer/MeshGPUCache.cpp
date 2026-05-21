#include "MeshGPUCache.hpp"

#include <cstdint>
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
        if (assetManager == nullptr || device == nullptr || item.submission == nullptr)
        {
            return nullptr;
        }

        const auto cached = m_MeshCache.find(item.primitiveKey);
        if (cached != m_MeshCache.end())
        {
            return &cached->second;
        }

        const std::string meshResourcePath = BuildMeshResourcePath(item);
        const std::shared_ptr<Mesh> mesh = assetManager->GetByPath<Mesh>(meshResourcePath);
        if (mesh == nullptr || item.submission->primitiveIndex >= mesh->primitives.size())
        {
            if (m_MissingMeshWarnings.insert(item.primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Mesh GPU cache skipped '{}': resource not found or primitive index out of range. normalized='{}'.",
                                  BuildMeshPrimitiveDebugName(item),
                                  assetManager->NormalizePath(meshResourcePath));
            }
            return nullptr;
        }

        const MeshPrimitive &primitive = mesh->primitives[item.submission->primitiveIndex];
        if (!primitive.HasGeometry())
        {
            if (m_MissingMeshWarnings.insert(item.primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Mesh GPU cache skipped '{}': primitive has no decoded geometry.",
                                  BuildMeshPrimitiveDebugName(item));
            }
            return nullptr;
        }

        MeshGPUPrimitive gpuPrimitive{};
        gpuPrimitive.vertexBuffer = device->CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(primitive.vertices, RHI::BufferUsage::Vertex));
        gpuPrimitive.indexBuffer = device->CreateBuffer(
            MeshGPUCacheDetail::StaticBufferDesc(primitive.indices, RHI::BufferUsage::Index));
        gpuPrimitive.indexCount = static_cast<std::uint32_t>(primitive.indices.size());

        if (gpuPrimitive.vertexBuffer == nullptr || gpuPrimitive.indexBuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Mesh GPU cache failed to upload '{}'.", BuildMeshPrimitiveDebugName(item));
            return nullptr;
        }

        if (stats != nullptr)
        {
            const std::uint64_t vertexBytes = static_cast<std::uint64_t>(primitive.vertices.size() * sizeof(MeshVertex));
            const std::uint64_t indexBytes = static_cast<std::uint64_t>(primitive.indices.size() * sizeof(std::uint32_t));
            ++stats->meshUploads;
            stats->meshUploadBytes += vertexBytes + indexBytes;
        }

        PHYSARA_CORE_INFO("Mesh GPU cache uploaded '{}': vertices={}, indices={}.",
                          BuildMeshPrimitiveDebugName(item),
                          primitive.vertices.size(),
                          primitive.indices.size());
        auto [inserted, _] = m_MeshCache.emplace(item.primitiveKey, std::move(gpuPrimitive));
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