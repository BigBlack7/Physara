#pragma once

#include <filesystem>

namespace Physara::Engine
{
    class Scene;

    class SceneSerializer final
    {
    public:
        static bool Serialize(const Scene &scene, const std::filesystem::path &path);
        static bool Deserialize(Scene &scene, const std::filesystem::path &path);
    };
}