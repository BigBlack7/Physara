#pragma once

#include <filesystem>
#include <memory>

#include <Engine/Resource/Types/Texture.hpp>

namespace Physara::Engine
{
    class TextureLoader final
    {
    public:
        static std::shared_ptr<Texture> LoadRGBA8(const std::filesystem::path &path);
    };
}