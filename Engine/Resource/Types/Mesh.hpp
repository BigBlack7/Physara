#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct MeshPrimitive
    {
        std::uint32_t primitiveIndex{0};
        std::uint32_t materialIndex{0};
        std::uint64_t vertexCount{0};
        std::uint64_t indexCount{0};
        glm::vec3 boundsMin{0.f};
        glm::vec3 boundsMax{0.f};
        bool hasBounds{false};
    };

    struct Mesh
    {
        std::string path{};
        std::uint32_t meshIndex{0};
        std::string name{};
        std::vector<MeshPrimitive> primitives{};
    };
}