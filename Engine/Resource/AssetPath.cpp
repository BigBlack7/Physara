#include "AssetPath.hpp"

#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine::AssetPath
{
    bool IsSceneFile(const std::filesystem::path &path)
    {
        return Platform::FileSystem::NormalizeForCompare(path.filename().string()).ends_with(".scene.json");
    }

    bool IsModelFile(const std::filesystem::path &path)
    {
        const std::string extension = Platform::FileSystem::GetExtensionLower(path.string());
        return extension == ".gltf" || extension == ".glb";
    }

    bool IsTextureFile(const std::filesystem::path &path)
    {
        const std::string extension = Platform::FileSystem::GetExtensionLower(path.string());
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".exr";
    }

    bool IsShaderFile(const std::filesystem::path &path)
    {
        const std::string extension = Platform::FileSystem::GetExtensionLower(path.string());
        return extension == ".glsl" || extension == ".vert" || extension == ".frag" || extension == ".comp";
    }

    AssetKind Classify(const std::filesystem::path &path, bool isDirectory)
    {
        if (isDirectory)
        {
            return AssetKind::Folder;
        }
        if (IsSceneFile(path))
        {
            return AssetKind::Scene;
        }
        if (IsModelFile(path))
        {
            return AssetKind::Mesh;
        }
        if (IsTextureFile(path))
        {
            return AssetKind::Texture;
        }
        if (IsShaderFile(path))
        {
            return AssetKind::Shader;
        }
        return AssetKind::File;
    }
}