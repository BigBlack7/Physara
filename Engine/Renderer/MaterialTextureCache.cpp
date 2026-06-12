#include "MaterialTextureCache.hpp"

#include <algorithm>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
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
        m_TextureCache.clear();
        m_MissingTextureWarnings.clear();
        m_Bindings.clear();
        m_TextureBindingFrameIndex = 0u;
    }

    void MaterialTextureCache::Update(
        RHI::RHIDevice &device,
        RHI::RHICommandList &commandList,
        AssetManager *assetManager,
        const FrameData &frameData,
        FrameStatistics *stats)
    {
        if (m_TextureBindingFrameIndex == frameData.frameIndex)
        {
            return;
        }

        EnsureDefaults(device);
        if (frameData.materials.empty())
        {
            m_Bindings.clear();
            m_TextureBindingFrameIndex = frameData.frameIndex;
            return;
        }

        if (m_Bindings.size() != frameData.materials.size())
        {
            m_Bindings.resize(frameData.materials.size());
        }
        for (std::size_t i = 0; i < frameData.materials.size(); ++i)
        {
            const MaterialComponent &material = frameData.materials[i];
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

            MaterialTextureBinding binding{};
            binding.textures = {
                baseColor != nullptr ? baseColor : m_FallbackWhiteTexture.get(),
                metallicRoughness != nullptr ? metallicRoughness : m_FallbackWhiteTexture.get(),
                normal != nullptr ? normal : m_FallbackNormalTexture.get(),
                occlusion != nullptr ? occlusion : m_FallbackWhiteTexture.get(),
                emissive != nullptr ? emissive : m_FallbackWhiteTexture.get()};
            binding.sampler = m_LinearRepeatSampler.get();
            m_Bindings[i] = binding;
        }
        m_TextureBindingFrameIndex = frameData.frameIndex;
    }

    const MaterialTextureBinding *MaterialTextureCache::GetBinding(std::uint32_t materialIndex) const
    {
        if (materialIndex >= m_Bindings.size())
        {
            return nullptr;
        }

        return &m_Bindings[materialIndex];
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