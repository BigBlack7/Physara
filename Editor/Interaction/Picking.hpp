#pragma once

#include <glm/vec2.hpp>

#include <Editor/Core/EditorContext.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Editor
{
    struct PickingRequest
    {
        glm::vec2 viewportPosition{0.f};
        bool appendSelection{false};
        bool toggleSelection{false};
    };

    class Picking final
    {
    public:
        [[nodiscard]] Engine::EntityId Pick(const EditorContext &context, const PickingRequest &request) const;
        void Select(EditorContext &context, Engine::EntityId entity, const PickingRequest &request) const;

    private:
        [[nodiscard]] Engine::EntityId PickMesh(const EditorContext &context, const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &closestT) const;
        [[nodiscard]] Engine::EntityId PickLightProxy(const EditorContext &context, const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &closestT) const;
    };
}