#include "Entity.hpp"

namespace Physara::Engine
{
    Entity::Entity(EntityId handle, entt::registry *registry)
        : m_Handle(handle), m_Registry(registry)
    {
    }

    bool Entity::IsValid() const
    {
        return m_Registry != nullptr && m_Handle != NullEntity && m_Registry->valid(m_Handle);
    }

    bool Entity::operator==(const Entity &other) const
    {
        return m_Handle == other.m_Handle && m_Registry == other.m_Registry;
    }
}