#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Physara::Engine
{
    struct MeshVertex
    {
        glm::vec3 position{0.f};
        glm::vec3 normal{0.f, 0.f, 1.f};
        glm::vec4 tangent{1.f, 0.f, 0.f, 1.f};
        glm::vec2 texCoord0{0.f};
    };

    struct MeshPrimitive
    {
        std::uint32_t primitiveIndex{0};
        std::uint32_t materialIndex{0};
        std::uint64_t vertexCount{0};
        std::uint64_t indexCount{0};
        glm::vec3 boundsMin{0.f};
        glm::vec3 boundsMax{0.f};
        bool hasBounds{false};
        std::vector<MeshVertex> vertices{};
        std::vector<std::uint32_t> indices{};

        [[nodiscard]] bool HasGeometry() const { return !vertices.empty() && !indices.empty(); }
    };

    struct Mesh
    {
        std::string path{};
        std::uint32_t meshIndex{0};
        std::string name{};
        std::vector<MeshPrimitive> primitives{};
    };
}