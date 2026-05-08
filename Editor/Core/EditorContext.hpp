#pragma once

#include <entt/entt.hpp>

namespace Physara::Engine
{
    class Scene;
}

namespace Physara::Editor
{
    struct EditorContext
    {
        entt::entity selectedEntity{entt::null};
        Engine::Scene *activeScene{nullptr};
    };
}