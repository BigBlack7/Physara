#pragma once

#include <glm/mat4x4.hpp>

#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class Scene;

    class TransformSystem final
    {
    public:
        static void Update(Scene &scene);

    private:
        [[nodiscard]] static bool HasDirtyTransforms(Scene &scene);
        static void UpdateRecursive(Scene &scene, EntityId entity, const glm::mat4 &parentWorldMatrix, bool parentDirty);
    };
}
