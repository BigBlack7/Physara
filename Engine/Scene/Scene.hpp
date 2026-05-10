#pragma once

#include <string_view>
#include <vector>

#include <entt/entt.hpp>

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
        void DestroyEntity(Entity entity);
        void DestroyEntity(EntityId entity);

        bool SetParent(Entity child, Entity parent);
        bool SetParent(EntityId child, EntityId parent);
        void ClearParent(Entity child);
        void ClearParent(EntityId child);

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
        void MarkWorldTransformDirty(EntityId entity);

    private:
        entt::registry m_Registry{};
    };
}