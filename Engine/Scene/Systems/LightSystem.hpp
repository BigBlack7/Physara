#pragma once

#include <vector>

#include <Engine/Renderer/FrameData.hpp>

namespace Physara::Engine
{
    class Scene;

    class LightSystem final
    {
    public:
        [[nodiscard]] static std::vector<LightData> Collect(Scene &scene);
    };
}
