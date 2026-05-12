#include "TextureLoader.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include <stb/stb_image.h>

#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace Internal
    {
        TextureSourceFormat DetectFormat(const std::filesystem::path &path)
        {
            const std::string extension = Platform::FileSystem::GetExtensionLower(path.string());
            if (extension == ".png")
            {
                return TextureSourceFormat::PNG;
            }
            if (extension == ".jpg" || extension == ".jpeg")
            {
                return TextureSourceFormat::JPG;
            }
            if (extension == ".exr")
            {
                return TextureSourceFormat::EXR;
            }
            return TextureSourceFormat::Unknown;
        }
    }

    std::shared_ptr<Texture> TextureLoader::LoadRGBA8(const std::filesystem::path &path)
    {
        std::vector<std::uint8_t> fileData;
        try
        {
            fileData = Platform::FileSystem::ReadBinaryFile(path.string());
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_WARN("Failed to read texture '{}': {}", path.string(), error.what());
            return {};
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load_from_memory(fileData.data(),
                                                      static_cast<int>(fileData.size()),
                                                      &width,
                                                      &height,
                                                      &channels,
                                                      4);
        if (pixels == nullptr)
        {
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->path = path.lexically_normal().generic_string();
        texture->width = static_cast<std::uint32_t>(std::max(width, 0));
        texture->height = static_cast<std::uint32_t>(std::max(height, 0));
        texture->channels = 4;
        texture->sourceFormat = Internal::DetectFormat(path);
        texture->rgba8Pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);

        stbi_image_free(pixels);
        return texture;
    }
}