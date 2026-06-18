#pragma once

#include <string_view>
#include <vector>

#include <entt/entt.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <Engine/Scene/Entity.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity CreateEntity(std::string_view name = "Entity");
        Entity EnsureSceneCamera();
        [[nodiscard]] Entity GetSceneCameraEntity();
        [[nodiscard]] EntityId GetSceneCameraEntityId() const;
        [[nodiscard]] bool IsSceneCamera(EntityId entity) const;
        void DestroyEntity(Entity entity);
        void DestroyEntity(EntityId entity);
        void Clear();

        bool SetParent(Entity child, Entity parent);
        bool SetParent(EntityId child, EntityId parent);
        void ClearParent(Entity child);
        void ClearParent(EntityId child);
        bool SetWorldTransform(EntityId entity, const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale);
        bool SetWorldMatrix(EntityId entity, const glm::mat4 &worldMatrix);

        [[nodiscard]] bool IsValid(EntityId entity) const;
        [[nodiscard]] Entity GetEntity(EntityId entity);
        [[nodiscard]] std::vector<Entity> GetRootEntities();

        void UpdateTransforms();

        [[nodiscard]] entt::registry &GetRegistry() { return m_Registry; }
        [[nodiscard]] const entt::registry &GetRegistry() const { return m_Registry; }

    private:
        void DetachFromParent(EntityId child);
        void AttachToParent(EntityId child, EntityId parent);
        bool WouldCreateCycle(EntityId child, EntityId parent) const;
        [[nodiscard]] glm::mat4 GetParentWorldMatrix(EntityId entity) const;
        void MarkWorldTransformDirty(EntityId entity);

    private:
        entt::registry m_Registry{};
    };
}