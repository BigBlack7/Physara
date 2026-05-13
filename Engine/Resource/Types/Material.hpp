#pragma once

#include <cstdint>
#include <string>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Components/MaterialComponent.hpp>

namespace Physara::Engine
{
    struct Material
    {
        std::string path{};
        std::string name{};
        std::uint32_t materialIndex{0};

        ShadingModel shadingModel{ShadingModel::Lit};
        AlphaMode alphaMode{AlphaMode::Opaque};
        bool doubleSided{false};
        bool castShadow{true};

        glm::vec4 baseColor{1.f};
        float metallic{1.f};
        float roughness{1.f};
        float ambientOcclusion{1.f};
        float alphaCutoff{0.5f};
        glm::vec3 emissiveColor{0.f};
        float normalScale{1.f};

        TextureSlot baseColorTexture{};
        TextureSlot metallicRoughnessTexture{};
        TextureSlot normalTexture{};
        TextureSlot occlusionTexture{};
        TextureSlot emissiveTexture{};
    };
}