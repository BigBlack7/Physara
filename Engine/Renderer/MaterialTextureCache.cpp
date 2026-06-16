#include "MaterialTextureCache.hpp"

#include <algorithm>
#include <limits>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/UploadHasher.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Texture.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

namespace Physara::Engine
{
    namespace MaterialTextureCacheDetail
    {
        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        RHI::RHITextureDesc TextureDesc(
            std::uint32_t width,
            std::uint32_t height,
            const void *pixels,
            std::uint32_t mipLevels = 1u,
            RHI::TextureColorSpace colorSpace = RHI::TextureColorSpace::Linear)
        {
            RHI::RHITextureDesc desc{};
            desc.width = MaxValue(width, 1u);
            desc.height = MaxValue(height, 1u);
            desc.mipLevels = MaxValue(mipLevels, 1u);
            desc.format = RHI::TextureFormat::RGBA8;
            desc.colorSpace = colorSpace;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = pixels;
            return desc;
        }

        std::uint32_t CalculateMipLevels(std::uint32_t width, std::uint32_t height)
        {
            std::uint32_t levels = 1u;
            std::uint32_t size = MaxValue(width, height);
            while (size > 1u)
            {
                size >>= 1u;
                ++levels;
            }
            return levels;
        }

    }

    void MaterialTextureCache::Reset()
    {
        m_LinearRepeatSampler.reset();
        m_FallbackWhiteTexture.reset();
        m_FallbackNormalTexture.reset();
        m_MaterialTextureIndexBuffer.reset();
        m_BindlessTextureHandleBuffer.reset();
        m_TextureCache.clear();
        m_MissingTextureWarnings.clear();
        m_ResourceSets.clear();
        m_FrameResourceSets.clear();
        m_TextureSets.clear();
        m_FrameTextureIndices.clear();
        m_BindlessTextureHandles.clear();
        m_BindlessTextureHandleEntries.clear();
        m_NextTextureSetId = 1u;
        m_ResourceSetFrameIndex = std::numeric_limits<std::uint64_t>::max();
        m_BindlessUploadSignature = std::numeric_limits<std::uint64_t>::max();
        m_BindlessReady = false;
    }

    void MaterialTextureCache::Update(
        RHI::RHIDevice &device,
        RHI::RHICommandList &commandList,
        AssetManager *assetManager,
        const FrameData &frameData,
        FrameStatistics *stats)
    {
        if (m_ResourceSetFrameIndex == frameData.frameIndex)
        {
            return;
        }

        EnsureDefaults(device);
        m_BindlessReady = false;
        if (frameData.materials.empty())
        {
            m_FrameResourceSets.clear();
            m_FrameTextureIndices.clear();
            m_BindlessTextureHandles.clear();
            m_BindlessTextureHandleEntries.clear();
            m_ResourceSetFrameIndex = frameData.frameIndex;
            if (stats != nullptr)
            {
                stats->materialResourceSets = static_cast<std::uint32_t>(m_ResourceSets.size());
            }
            return;
        }

        if (m_FrameResourceSets.size() != frameData.materials.size())
        {
            m_FrameResourceSets.resize(frameData.materials.size());
        }
        for (std::size_t i = 0; i < frameData.materials.size(); ++i)
        {
            const MaterialComponent &material = frameData.materials[i];
            const MaterialInstanceId materialInstanceId = i < frameData.materialInstanceIds.size()
                                                              ? frameData.materialInstanceIds[i]
                                                              : InvalidMaterialInstanceId;
            m_FrameResourceSets[i] = GetOrCreateResourceSet(
                device,
                commandList,
                assetManager,
                materialInstanceId,
                material,
                stats);
        }
        if (stats != nullptr)
        {
            stats->materialResourceSets = static_cast<std::uint32_t>(m_ResourceSets.size());
        }
        UpdateBindlessTables(device, frameData, stats);
        m_ResourceSetFrameIndex = frameData.frameIndex;
    }

    const MaterialResourceSet *MaterialTextureCache::GetResourceSet(std::uint32_t materialIndex) const
    {
        if (materialIndex >= m_FrameResourceSets.size())
        {
            return nullptr;
        }

        return m_FrameResourceSets[materialIndex];
    }

    const MaterialResourceSet *MaterialTextureCache::GetOrCreateResourceSet(
        RHI::RHIDevice &device,
        RHI::RHICommandList &commandList,
        AssetManager *assetManager,
        MaterialInstanceId materialInstanceId,
        const MaterialComponent &material,
        FrameStatistics *stats)
    {
        if (materialInstanceId == InvalidMaterialInstanceId)
        {
            return nullptr;
        }

        auto found = m_ResourceSets.find(materialInstanceId);
        if (found != m_ResourceSets.end() && found->second.complete)
        {
            return &found->second;
        }

        MaterialResourceSet resourceSet = BuildResourceSet(
            device,
            commandList,
            assetManager,
            materialInstanceId,
            material,
            stats);
        if (found == m_ResourceSets.end())
        {
            auto [inserted, _] = m_ResourceSets.emplace(materialInstanceId, resourceSet);
            return &inserted->second;
        }

        found->second = resourceSet;
        return &found->second;
    }

    MaterialResourceSet MaterialTextureCache::BuildResourceSet(
        RHI::RHIDevice &device,
        RHI::RHICommandList &commandList,
        AssetManager *assetManager,
        MaterialInstanceId materialInstanceId,
        const MaterialComponent &material,
        FrameStatistics *stats)
    {
        MaterialResourceSet resourceSet{};
        resourceSet.materialInstanceId = materialInstanceId;
        resourceSet.sampler = m_LinearRepeatSampler.get();
        resourceSet.complete = true;

        RHI::RHITexture *baseColor = GetOrCreateTexture(
            device,
            commandList,
            assetManager,
            material.baseColorTexture.path,
            RHI::TextureColorSpace::SRGB,
            stats);
        RHI::RHITexture *metallicRoughness = GetOrCreateTexture(
            device,
            commandList,
            assetManager,
            material.metallicRoughnessTexture.path,
            RHI::TextureColorSpace::Linear,
            stats);
        RHI::RHITexture *normal = GetOrCreateTexture(
            device,
            commandList,
            assetManager,
            material.normalTexture.path,
            RHI::TextureColorSpace::Linear,
            stats);
        RHI::RHITexture *occlusion = GetOrCreateTexture(
            device,
            commandList,
            assetManager,
            material.occlusionTexture.path,
            RHI::TextureColorSpace::Linear,
            stats);
        RHI::RHITexture *emissive = GetOrCreateTexture(
            device,
            commandList,
            assetManager,
            material.emissiveTexture.path,
            RHI::TextureColorSpace::SRGB,
            stats);

        resourceSet.complete = (!material.baseColorTexture.IsBound() || baseColor != nullptr) &&
                               (!material.metallicRoughnessTexture.IsBound() || metallicRoughness != nullptr) &&
                               (!material.normalTexture.IsBound() || normal != nullptr) &&
                               (!material.occlusionTexture.IsBound() || occlusion != nullptr) &&
                               (!material.emissiveTexture.IsBound() || emissive != nullptr);
        resourceSet.textureBindings = {
            RHI::RHITextureBinding{
                Binding(GPUTextureBinding::BaseColor),
                baseColor != nullptr ? baseColor : m_FallbackWhiteTexture.get(),
                resourceSet.sampler},
            RHI::RHITextureBinding{
                Binding(GPUTextureBinding::MetallicRoughness),
                metallicRoughness != nullptr ? metallicRoughness : m_FallbackWhiteTexture.get(),
                resourceSet.sampler},
            RHI::RHITextureBinding{
                Binding(GPUTextureBinding::Normal),
                normal != nullptr ? normal : m_FallbackNormalTexture.get(),
                resourceSet.sampler},
            RHI::RHITextureBinding{
                Binding(GPUTextureBinding::Occlusion),
                occlusion != nullptr ? occlusion : m_FallbackWhiteTexture.get(),
                resourceSet.sampler},
            RHI::RHITextureBinding{
                Binding(GPUTextureBinding::Emissive),
                emissive != nullptr ? emissive : m_FallbackWhiteTexture.get(),
                resourceSet.sampler}};
        resourceSet.textureSetId = ResolveTextureSetId(resourceSet.textureBindings, resourceSet.sampler);
        return resourceSet;
    }

    std::uint32_t MaterialTextureCache::ResolveTextureSetId(
        const std::array<RHI::RHITextureBinding, MaterialTextureSlotCount> &textureBindings,
        RHI::RHISampler *sampler)
    {
        for (const TextureSetEntry &entry : m_TextureSets)
        {
            if (entry.sampler != sampler)
            {
                continue;
            }

            bool sameTextures = true;
            for (std::size_t i = 0; i < textureBindings.size(); ++i)
            {
                if (entry.textures[i] != textureBindings[i].texture)
                {
                    sameTextures = false;
                    break;
                }
            }
            if (sameTextures)
            {
                return entry.id;
            }
        }

        TextureSetEntry entry{};
        for (std::size_t i = 0; i < textureBindings.size(); ++i)
        {
            entry.textures[i] = textureBindings[i].texture;
        }
        entry.sampler = sampler;
        entry.id = m_NextTextureSetId++;
        m_TextureSets.push_back(entry);
        return entry.id;
    }

    void MaterialTextureCache::UpdateBindlessTables(
        RHI::RHIDevice &device,
        const FrameData &frameData,
        FrameStatistics *stats)
    {
        if (!device.SupportsBindlessTextures() || m_FrameResourceSets.empty())
        {
            m_BindlessReady = false;
            return;
        }

        m_FrameTextureIndices.clear();
        m_FrameTextureIndices.resize(frameData.materials.size());
        m_BindlessTextureHandles.clear();
        m_BindlessTextureHandles.push_back(BindlessTextureHandleData{});
        m_BindlessTextureHandleEntries.clear();

        bool complete = true;
        for (std::size_t materialIndex = 0; materialIndex < m_FrameResourceSets.size(); ++materialIndex)
        {
            const MaterialResourceSet *resourceSet = m_FrameResourceSets[materialIndex];
            if (resourceSet == nullptr)
            {
                complete = false;
                continue;
            }

            MaterialTextureIndexData indices{};
            for (std::size_t slot = 0; slot < resourceSet->textureBindings.size(); ++slot)
            {
                const RHI::RHITextureBinding &binding = resourceSet->textureBindings[slot];
                const std::uint32_t index = ResolveBindlessTextureIndex(device, binding.texture, binding.sampler);
                if (index == 0u)
                {
                    complete = false;
                }
                if (slot < 4u)
                {
                    indices.slots0[static_cast<glm::length_t>(slot)] = index;
                }
                else
                {
                    indices.slots1[static_cast<glm::length_t>(slot - 4u)] = index;
                }
            }
            m_FrameTextureIndices[materialIndex] = indices;
        }

        if (!complete || m_BindlessTextureHandles.size() <= 1u || m_FrameTextureIndices.empty())
        {
            m_BindlessReady = false;
            return;
        }

        const std::uint64_t signature = BuildBindlessUploadSignature();
        const std::uint32_t indexBytes =
            static_cast<std::uint32_t>(m_FrameTextureIndices.size() * sizeof(MaterialTextureIndexData));
        const std::uint32_t handleBytes =
            static_cast<std::uint32_t>(m_BindlessTextureHandles.size() * sizeof(BindlessTextureHandleData));
        if (signature != m_BindlessUploadSignature ||
            m_MaterialTextureIndexBuffer == nullptr ||
            m_MaterialTextureIndexBuffer->GetSize() < indexBytes ||
            m_BindlessTextureHandleBuffer == nullptr ||
            m_BindlessTextureHandleBuffer->GetSize() < handleBytes)
        {
            UploadBindlessStorageBuffer(
                device,
                m_MaterialTextureIndexBuffer,
                m_FrameTextureIndices.data(),
                indexBytes,
                stats);
            UploadBindlessStorageBuffer(
                device,
                m_BindlessTextureHandleBuffer,
                m_BindlessTextureHandles.data(),
                handleBytes,
                stats);
            m_BindlessUploadSignature = signature;
        }

        m_BindlessReady = m_MaterialTextureIndexBuffer != nullptr && m_BindlessTextureHandleBuffer != nullptr;
    }

    std::uint32_t MaterialTextureCache::ResolveBindlessTextureIndex(
        RHI::RHIDevice &device,
        RHI::RHITexture *texture,
        RHI::RHISampler *sampler)
    {
        if (texture == nullptr)
        {
            return 0u;
        }

        for (const BindlessTextureHandleEntry &entry : m_BindlessTextureHandleEntries)
        {
            if (entry.texture == texture && entry.sampler == sampler)
            {
                return entry.index;
            }
        }

        const std::uint64_t handle = device.GetBindlessTextureHandle(texture, sampler);
        if (handle == 0u)
        {
            return 0u;
        }

        BindlessTextureHandleData data{};
        data.words.x = static_cast<std::uint32_t>(handle & 0xffffffffull);
        data.words.y = static_cast<std::uint32_t>(handle >> 32u);
        const std::uint32_t index = static_cast<std::uint32_t>(m_BindlessTextureHandles.size());
        m_BindlessTextureHandles.push_back(data);
        m_BindlessTextureHandleEntries.push_back(BindlessTextureHandleEntry{texture, sampler, index});
        return index;
    }

    void MaterialTextureCache::UploadBindlessStorageBuffer(
        RHI::RHIDevice &device,
        std::unique_ptr<RHI::RHIBuffer> &buffer,
        const void *data,
        std::uint32_t size,
        FrameStatistics *stats)
    {
        if (data == nullptr || size == 0u)
        {
            return;
        }

        if (buffer == nullptr || buffer->GetSize() < size)
        {
            std::uint32_t capacity = 256u;
            while (capacity < size)
            {
                capacity *= 2u;
            }

            RHI::RHIBufferDesc desc{};
            desc.size = capacity;
            desc.usage = RHI::BufferUsage::Storage;
            desc.dynamic = true;
            buffer = device.CreateBuffer(desc);
        }
        if (buffer == nullptr)
        {
            return;
        }

        buffer->UploadData(data, size);
        if (stats != nullptr)
        {
            stats->bufferUploadBytes += size;
            ++stats->bufferUploadChunks;
        }
    }

    std::uint64_t MaterialTextureCache::BuildBindlessUploadSignature() const
    {
        std::uint64_t hash = UploadHash::Offset;
        hash = UploadHash::Vector(hash, m_FrameTextureIndices);
        hash = UploadHash::Vector(hash, m_BindlessTextureHandles);
        return hash;
    }

    void MaterialTextureCache::EnsureDefaults(RHI::RHIDevice &device)
    {
        if (m_LinearRepeatSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::Repeat;
            desc.wrapV = RHI::WrapMode::Repeat;
            desc.wrapW = RHI::WrapMode::Repeat;
            desc.anisotropy = static_cast<float>(MaterialTextureCacheDetail::MaxValue(device.GetMaxAnisotropy(), 1));
            m_LinearRepeatSampler = device.CreateSampler(desc);
        }

        if (m_FallbackWhiteTexture == nullptr)
        {
            const std::uint8_t white[4]{255u, 255u, 255u, 255u};
            m_FallbackWhiteTexture = device.CreateTexture(MaterialTextureCacheDetail::TextureDesc(1u, 1u, white));
        }

        if (m_FallbackNormalTexture == nullptr)
        {
            const std::uint8_t normal[4]{128u, 128u, 255u, 255u};
            m_FallbackNormalTexture = device.CreateTexture(MaterialTextureCacheDetail::TextureDesc(1u, 1u, normal));
        }
    }

    RHI::RHITexture *MaterialTextureCache::GetOrCreateTexture(
        RHI::RHIDevice &device,
        RHI::RHICommandList &commandList,
        AssetManager *assetManager,
        const std::string &texturePath,
        RHI::TextureColorSpace colorSpace,
        FrameStatistics *stats)
    {
        if (texturePath.empty() || assetManager == nullptr)
        {
            return nullptr;
        }

        const std::string normalizedPath = assetManager->NormalizePath(texturePath);
        const std::string cacheKey = normalizedPath + (colorSpace == RHI::TextureColorSpace::SRGB ? "|srgb" : "|linear");
        const auto cached = m_TextureCache.find(cacheKey);
        if (cached != m_TextureCache.end())
        {
            return cached->second.texture.get();
        }

        const std::shared_ptr<Texture> texture = assetManager->GetByPath<Texture>(normalizedPath);
        if (texture == nullptr || !texture->IsLoaded() || texture->rgba8Pixels.empty())
        {
            if (m_MissingTextureWarnings.insert(normalizedPath).second)
            {
                PHYSARA_CORE_WARN("Material texture '{}' is not loaded; using fallback texture.", normalizedPath);
            }
            return nullptr;
        }

        const std::uint32_t mipLevels = MaterialTextureCacheDetail::CalculateMipLevels(texture->width, texture->height);
        TextureGPUResource resource{};
        resource.texture = device.CreateTexture(
            MaterialTextureCacheDetail::TextureDesc(texture->width, texture->height, texture->rgba8Pixels.data(), mipLevels, colorSpace));
        if (resource.texture == nullptr)
        {
            PHYSARA_CORE_ERROR("Material texture cache failed to upload '{}'.", normalizedPath);
            return nullptr;
        }

        if (stats != nullptr)
        {
            ++stats->textureUploads;
            stats->textureUploadBytes += texture->rgba8Pixels.size();
        }

        if (mipLevels > 1u)
        {
            commandList.GenerateMipmaps(resource.texture.get());
            resource.generatedMipmaps = true;
        }

        PHYSARA_CORE_INFO("Material texture uploaded '{}': {}x{}, mips={}.",
                          normalizedPath,
                          texture->width,
                          texture->height,
                          mipLevels);
        auto [inserted, _] = m_TextureCache.emplace(cacheKey, std::move(resource));
        return inserted->second.texture.get();
    }
}