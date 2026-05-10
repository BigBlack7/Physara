#pragma once

#include <entt/entt.hpp>

namespace Physara::Engine
{
    using EntityId = entt::entity;

    inline constexpr EntityId NullEntity = entt::null;
}