#include "MaterialSignature.hpp"

#include <string_view>

#include <Engine/Renderer/UploadHasher.hpp>

namespace Physara::Engine
{
    namespace MaterialSignatureDetail
    {
        std::uint64_t HashString(std::string_view value)
        {
            return UploadHash::String(UploadHash::Offset, value);
        }

        void HashCombine(std::uint64_t &seed, std::uint64_t value)
        {
            seed = UploadHash::Value(seed, value);
        }

        void HashCombine(std::uint64_t &seed, std::string_view value)
        {
            HashCombine(seed, HashString(value));
        }

        void HashCombine(std::uint64_t &seed, float value)
        {
            seed = UploadHash::Value(seed, value);
        }

        void HashCombine(std::uint64_t &seed, const glm::vec3 &value)
        {
            HashCombine(seed, value.x);
            HashCombine(seed, value.y);
            HashCombine(seed, value.z);
        }

        void HashCombine(std::uint64_t &seed, const glm::vec4 &value)
        {
            HashCombine(seed, value.x);
            HashCombine(seed, value.y);
            HashCombine(seed, value.z);
            HashCombine(seed, value.w);
        }

        void HashCombine(std::uint64_t &seed, const TextureSlot &slot)
        {
            HashCombine(seed, slot.path);
            HashCombine(seed, static_cast<std::uint64_t>(slot.texCoord));
        }

        bool TextureSlotEquals(const TextureSlot &lhs, const TextureSlot &rhs)
        {
            return lhs.path == rhs.path && lhs.texCoord == rhs.texCoord;
        }
    }

    std::uint64_t MaterialSignature::Build(const MaterialComponent &material)
    {
        std::uint64_t seed = MaterialSignatureDetail::HashString(material.materialPath);
        MaterialSignatureDetail::HashCombine(seed, static_cast<std::uint64_t>(material.shadingModel));
        MaterialSignatureDetail::HashCombine(seed, static_cast<std::uint64_t>(material.alphaMode));
        MaterialSignatureDetail::HashCombine(seed, material.doubleSided ? 1ull : 0ull);
        MaterialSignatureDetail::HashCombine(seed, material.castShadow ? 1ull : 0ull);
        MaterialSignatureDetail::HashCombine(seed, material.baseColor);
        MaterialSignatureDetail::HashCombine(seed, material.metallic);
        MaterialSignatureDetail::HashCombine(seed, material.roughness);
        MaterialSignatureDetail::HashCombine(seed, material.reflectance);
        MaterialSignatureDetail::HashCombine(seed, material.ambientOcclusion);
        MaterialSignatureDetail::HashCombine(seed, material.alphaCutoff);
        MaterialSignatureDetail::HashCombine(seed, material.metallicTextureInfluence);
        MaterialSignatureDetail::HashCombine(seed, material.roughnessTextureInfluence);
        MaterialSignatureDetail::HashCombine(seed, material.ambientOcclusionTextureInfluence);
        MaterialSignatureDetail::HashCombine(seed, material.emissiveColor);
        MaterialSignatureDetail::HashCombine(seed, material.emissiveLuminance);
        MaterialSignatureDetail::HashCombine(seed, material.normalScale);
        MaterialSignatureDetail::HashCombine(seed, material.flipNormalY ? 1ull : 0ull);
        MaterialSignatureDetail::HashCombine(seed, material.baseColorTexture);
        MaterialSignatureDetail::HashCombine(seed, material.metallicRoughnessTexture);
        MaterialSignatureDetail::HashCombine(seed, material.normalTexture);
        MaterialSignatureDetail::HashCombine(seed, material.occlusionTexture);
        MaterialSignatureDetail::HashCombine(seed, material.emissiveTexture);
        return seed;
    }

    bool MaterialSignature::Equals(const MaterialComponent &lhs, const MaterialComponent &rhs)
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
               MaterialSignatureDetail::TextureSlotEquals(lhs.baseColorTexture, rhs.baseColorTexture) &&
               MaterialSignatureDetail::TextureSlotEquals(lhs.metallicRoughnessTexture, rhs.metallicRoughnessTexture) &&
               MaterialSignatureDetail::TextureSlotEquals(lhs.normalTexture, rhs.normalTexture) &&
               MaterialSignatureDetail::TextureSlotEquals(lhs.occlusionTexture, rhs.occlusionTexture) &&
               MaterialSignatureDetail::TextureSlotEquals(lhs.emissiveTexture, rhs.emissiveTexture);
    }
}
