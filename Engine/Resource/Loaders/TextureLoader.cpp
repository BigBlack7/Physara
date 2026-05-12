#include "TextureLoader.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include <stb/stb_image.h>

namespace Physara::Engine
{
    namespace Internal
    {
        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        TextureSourceFormat DetectFormat(const std::filesystem::path &path)
        {
            const std::string extension = ToLower(path.extension().string());
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
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
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