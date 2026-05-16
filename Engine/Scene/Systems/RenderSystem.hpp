#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class Scene;

    struct RenderMeshSubmission
    {
        EntityId entity{NullEntity};
        std::string meshPath{};
        std::uint32_t meshIndex{0};
        std::uint32_t primitiveIndex{0};
        std::string materialPath{};
        MaterialComponent material{};
        glm::mat4 model{1.f};
        glm::mat4 inverseTransposeModel{1.f};
        glm::vec3 boundsCenter{0.f};
        float boundsRadius{0.f};
        bool hasBounds{false};
        bool receiveShadows{true};
    };

    class RenderSystem final
    {
    public:
        [[nodiscard]] static std::vector<RenderMeshSubmission> Collect(Scene &scene);
    };
}