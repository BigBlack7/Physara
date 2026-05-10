#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct MeshPrimitiveRef
    {
        std::string assetPath{};
        std::uint32_t meshIndex{0};
        std::uint32_t primitiveIndex{0};

        MeshPrimitiveRef() = default;
        explicit MeshPrimitiveRef(std::string_view path) : assetPath(path) {}
        MeshPrimitiveRef(std::string path, std::uint32_t mesh, std::uint32_t primitive = 0)
            : assetPath(std::move(path)), meshIndex(mesh), primitiveIndex(primitive) {}

        [[nodiscard]] bool IsValid() const { return !assetPath.empty(); }
    };

    struct MaterialSlotRef
    {
        std::string materialPath{};
        std::uint32_t slotIndex{0};

        MaterialSlotRef() = default;
        MaterialSlotRef(std::uint32_t slot, std::string path = {}) : materialPath(std::move(path)), slotIndex(slot) {}

        [[nodiscard]] bool HasOverride() const { return !materialPath.empty(); }
    };

    struct MeshBounds
    {
        glm::vec3 min{0.f, 0.f, 0.f};
        glm::vec3 max{0.f, 0.f, 0.f};
        glm::vec3 center{0.f, 0.f, 0.f};
        float radius{0.f};
        bool valid{false};
    };

    struct MeshComponent
    {
        MeshPrimitiveRef primitive{};
        std::vector<MaterialSlotRef> materialSlots{};
        MeshBounds localBounds{};
        bool visible{true};
        bool receiveShadows{true};

        MeshComponent() = default;
        explicit MeshComponent(std::string_view assetPath) : primitive(assetPath) {}
        MeshComponent(std::string assetPath, std::uint32_t meshIndex, std::uint32_t primitiveIndex = 0)
            : primitive(std::move(assetPath), meshIndex, primitiveIndex) {}

        [[nodiscard]] bool HasMesh() const { return primitive.IsValid(); }
        [[nodiscard]] std::size_t GetMaterialSlotCount() const { return materialSlots.size(); }
    };
}