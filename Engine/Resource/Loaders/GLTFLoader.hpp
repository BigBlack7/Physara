#pragma once

#include <filesystem>

#include <Engine/Scene/Entity.hpp>

namespace Physara::Engine
{
    class AssetManager;
    class Scene;

    class GLTFLoader final
    {
    public:
        static bool LoadResources(const std::filesystem::path &path, AssetManager *assetManager);
        static Entity LoadToScene(Scene &scene, const std::filesystem::path &path, AssetManager *assetManager = nullptr);
    };
}