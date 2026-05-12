#pragma once

#include <filesystem>

namespace Physara::Engine::AssetPath
{
    enum class AssetKind
    {
        Folder,
        Scene,
        Mesh,
        Texture,
        Shader,
        File
    };

    [[nodiscard]] bool IsSceneFile(const std::filesystem::path &path);
    [[nodiscard]] bool IsModelFile(const std::filesystem::path &path);
    [[nodiscard]] bool IsTextureFile(const std::filesystem::path &path);
    [[nodiscard]] bool IsShaderFile(const std::filesystem::path &path);
    [[nodiscard]] AssetKind Classify(const std::filesystem::path &path, bool isDirectory);
}