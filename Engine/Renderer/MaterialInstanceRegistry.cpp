#include "MaterialInstanceRegistry.hpp"

#include <functional>
#include <string_view>
#include <utility>

namespace Physara::Engine
{
    void MaterialInstanceRegistry::Reset()
    {
        m_Entries.clear();
        m_IdsBySignature.clear();
    }

    MaterialInstanceId MaterialInstanceRegistry::Resolve(const MaterialComponent &material)
    {
        const std::uint64_t signature = HashMaterial(material);
        auto &candidateIds = m_IdsBySignature[signature];
        for (MaterialInstanceId candidateId : candidateIds)
        {
            const MaterialComponent *candidate = Get(candidateId);
            if (candidate != nullptr && MaterialEquals(*candidate, material))
            {
                return candidateId;
            }
        }

        const MaterialInstanceId id = static_cast<MaterialInstanceId>(m_Entries.size());
        Entry entry{};
        entry.id = id;
        entry.signature = signature;
        entry.material = material;
        m_Entries.push_back(std::move(entry));
        candidateIds.push_back(id);
        return id;
    }

    const MaterialComponent *MaterialInstanceRegistry::Get(MaterialInstanceId id) const
    {
        if (id == InvalidMaterialInstanceId || id >= m_Entries.size())
        {
            return nullptr;
        }
        return &m_Entries[id].material;
    }

    std::uint64_t MaterialInstanceRegistry::GetSignature(MaterialInstanceId id) const
    {
        if (id == InvalidMaterialInstanceId || id >= m_Entries.size())
        {
            return 0u;
        }
        return m_Entries[id].signature;
    }

    std::uint64_t MaterialInstanceRegistry::HashMaterial(const MaterialComponent &material)
    {
        std::uint64_t seed = HashString(material.materialPath);
        HashCombine(seed, static_cast<std::uint64_t>(material.shadingModel));
        HashCombine(seed, static_cast<std::uint64_t>(material.alphaMode));
        HashCombine(seed, material.doubleSided ? 1ull : 0ull);
        HashCombine(seed, material.castShadow ? 1ull : 0ull);
        HashCombine(seed, material.baseColor);
        HashCombine(seed, material.metallic);
        HashCombine(seed, material.roughness);
        HashCombine(seed, material.reflectance);
        HashCombine(seed, material.ambientOcclusion);
        HashCombine(seed, material.alphaCutoff);
        HashCombine(seed, material.metallicTextureInfluence);
        HashCombine(seed, material.roughnessTextureInfluence);
        HashCombine(seed, material.ambientOcclusionTextureInfluence);
        HashCombine(seed, material.emissiveColor);
        HashCombine(seed, material.emissiveLuminance);
        HashCombine(seed, material.normalScale);
        HashCombine(seed, material.flipNormalY ? 1ull : 0ull);
        HashCombine(seed, material.baseColorTexture);
        HashCombine(seed, material.metallicRoughnessTexture);
        HashCombine(seed, material.normalTexture);
        HashCombine(seed, material.occlusionTexture);
        HashCombine(seed, material.emissiveTexture);
        return seed;
    }

    std::uint64_t MaterialInstanceRegistry::HashString(std::string_view value)
    {
        return static_cast<std::uint64_t>(std::hash<std::string_view>{}(value));
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, std::string_view value)
    {
        HashCombine(seed, HashString(value));
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, std::uint64_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, float value)
    {
        HashCombine(seed, static_cast<std::uint64_t>(std::hash<float>{}(value)));
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, const glm::vec3 &value)
    {
        HashCombine(seed, value.x);
        HashCombine(seed, value.y);
        HashCombine(seed, value.z);
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, const glm::vec4 &value)
    {
        HashCombine(seed, value.x);
        HashCombine(seed, value.y);
        HashCombine(seed, value.z);
        HashCombine(seed, value.w);
    }

    void MaterialInstanceRegistry::HashCombine(std::uint64_t &seed, const TextureSlot &slot)
    {
        HashCombine(seed, slot.path);
        HashCombine(seed, static_cast<std::uint64_t>(slot.texCoord));
    }

    bool MaterialInstanceRegistry::TextureSlotEquals(const TextureSlot &lhs, const TextureSlot &rhs)
    {
        return lhs.path == rhs.path && lhs.texCoord == rhs.texCoord;
    }

    bool MaterialInstanceRegistry::MaterialEquals(const MaterialComponent &lhs, const MaterialComponent &rhs)
    {
        return lhs.materialPath == rhs.materialPath &&
               lhs.shadingModel == rhs.shadingModel &&
               lhs.alphaMode == rhs.alphaMode &&
               lhs.doubleSided == rhs.doubleSided &&
               lhs.castShadow == rhs.castShadow &&
               lhs.baseColor == rhs.baseColor &&
               lhs.metallic == rhs.metallic &&
               lhs.roughness == rhs.roughness &&
               lhs.reflectance == rhs.reflectance &&
               lhs.ambientOcclusion == rhs.ambientOcclusion &&
               lhs.alphaCutoff == rhs.alphaCutoff &&
               lhs.metallicTextureInfluence == rhs.metallicTextureInfluence &&
               lhs.roughnessTextureInfluence == rhs.roughnessTextureInfluence &&
               lhs.ambientOcclusionTextureInfluence == rhs.ambientOcclusionTextureInfluence &&
               lhs.emissiveColor == rhs.emissiveColor &&
               lhs.emissiveLuminance == rhs.emissiveLuminance &&
               lhs.normalScale == rhs.normalScale &&
               lhs.flipNormalY == rhs.flipNormalY &&
               TextureSlotEquals(lhs.baseColorTexture, rhs.baseColorTexture) &&
               TextureSlotEquals(lhs.metallicRoughnessTexture, rhs.metallicRoughnessTexture) &&
               TextureSlotEquals(lhs.normalTexture, rhs.normalTexture) &&
               TextureSlotEquals(lhs.occlusionTexture, rhs.occlusionTexture) &&
               TextureSlotEquals(lhs.emissiveTexture, rhs.emissiveTexture);
    }
}