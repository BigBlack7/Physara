#pragma once

#include <cassert>
#include <utility>

#include <entt/entt.hpp>

#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class Entity
    {
    public:
        Entity() = default;
        Entity(EntityId handle, entt::registry *registry);

        template <typename T, typename... Args>
        T &AddComponent(Args &&...args)
        {
            assert(IsValid());
            assert(!HasComponent<T>()); // 检查实体是否已经拥有该类型的组件
            return m_Registry->emplace<T>(m_Handle, std::forward<Args>(args)...); // 在ECS注册表中为当前实体原地构造组件
        }

        template <typename T, typename... Args>
        T &AddOrReplaceComponent(Args &&...args)
        {
            assert(IsValid());
            return m_Registry->emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent() const
        {
            return IsValid() && m_Registry->all_of<T>(m_Handle);
        }

        template <typename T>
        T &GetComponent()
        {
            assert(HasComponent<T>());
            return m_Registry->get<T>(m_Handle);
        }

        template <typename T>
        const T &GetComponent() const
        {
            assert(HasComponent<T>());
            return m_Registry->get<T>(m_Handle);
        }

        template <typename T>
        void RemoveComponent()
        {
            assert(HasComponent<T>());
            m_Registry->remove<T>(m_Handle);
        }

        [[nodiscard]] EntityId GetHandle() const { return m_Handle; }
        [[nodiscard]] entt::registry *GetRegistry() const { return m_Registry; }
        [[nodiscard]] bool IsValid() const;

        explicit operator bool() const { return IsValid(); }
        bool operator==(const Entity &other) const;
        bool operator!=(const Entity &other) const { return !(*this == other); }

    private:
        EntityId m_Handle{NullEntity};
        entt::registry *m_Registry{nullptr};
    };
}