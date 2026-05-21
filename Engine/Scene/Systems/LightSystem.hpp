#pragma once

#include <vector>

#include <Engine/Renderer/FrameData.hpp>

namespace Physara::Engine
{
    class Scene;

    class LightSystem final
    {
    public:
        // Caller owns Scene::UpdateTransforms() so render and light collection can share one authoritative update.
        static void Collect(Scene &scene, std::vector<LightData> &lights);
    };
}