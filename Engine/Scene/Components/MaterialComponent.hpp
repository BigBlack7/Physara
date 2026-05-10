#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Physara::Engine
{
    enum class ShadingModel
    {
        Lit,
        Unlit
    };

    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    struct TextureSlot
    {
        std::string path{};
        std::uint32_t texCoord{0};

        TextureSlot() = default;
        explicit TextureSlot(std::string texturePath, std::uint32_t uvSet = 0)
            : path(std::move(texturePath)), texCoord(uvSet) {}

        [[nodiscard]] bool IsBound() const { return !path.empty(); }
    };

    struct MaterialComponent
    {
        std::string materialPath{};

        ShadingModel shadingModel{ShadingModel::Lit};
        AlphaMode alphaMode{AlphaMode::Opaque};
        bool doubleSided{false};
        bool castShadow{true};

        glm::vec4 baseColor{1.f, 1.f, 1.f, 1.f};
        float metallic{0.f};
        float roughness{0.5f};
        float ambientOcclusion{1.f};
        float alphaCutoff{0.5f};

        glm::vec3 emissiveColor{0.f, 0.f, 0.f};
        float emissiveLuminance{0.f}; // cd/m^2 for emissive surfaces.
        float normalScale{1.f};

        TextureSlot baseColorTexture{};
        TextureSlot metallicRoughnessTexture{};
        TextureSlot normalTexture{};
        TextureSlot occlusionTexture{};
        TextureSlot emissiveTexture{};

        MaterialComponent() = default;
        explicit MaterialComponent(std::string path)
            : materialPath(std::move(path)) {}

        void Sanitize()
        {
            metallic = std::clamp(metallic, 0.f, 1.f);
            roughness = std::clamp(roughness, 0.045f, 1.f);
            ambientOcclusion = std::clamp(ambientOcclusion, 0.f, 1.f);
            alphaCutoff = std::clamp(alphaCutoff, 0.f, 1.f);
            normalScale = std::max(normalScale, 0.f);
            emissiveLuminance = std::max(emissiveLuminance, 0.f);
            castShadow = shadingModel == ShadingModel::Unlit ? false : castShadow;
        }

        [[nodiscard]] bool IsTransparent() const { return alphaMode == AlphaMode::Blend; }
        [[nodiscard]] bool IsAlphaTested() const { return alphaMode == AlphaMode::Mask; }
        [[nodiscard]] bool HasResourceMaterial() const { return !materialPath.empty(); }
    };
}