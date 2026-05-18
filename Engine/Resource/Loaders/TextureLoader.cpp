#include "TextureLoader.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include <stb/stb_image.h>
#include <tinyexr/tinyexr.h>

#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace TextureLoaderDetail
    {
        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

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

        std::shared_ptr<Texture> LoadEXR32F(const std::filesystem::path &path, const std::vector<std::uint8_t> &fileData)
        {
            float *pixels = nullptr;
            int width = 0;
            int height = 0;
            const char *error = nullptr;
            const int result = LoadEXRFromMemory(&pixels, &width, &height, fileData.data(), fileData.size(), &error);
            if (result != TINYEXR_SUCCESS || pixels == nullptr || width <= 0 || height <= 0)
            {
                PHYSARA_CORE_WARN("Failed to load EXR pixels '{}': {}",
                                  path.string(),
                                  error != nullptr ? error : "Unknown EXR error");
                if (error != nullptr)
                {
                    FreeEXRErrorMessage(error);
                }
                return {};
            }

            if (error != nullptr)
            {
                FreeEXRErrorMessage(error);
            }

            auto texture = std::make_shared<Texture>();
            texture->path = path.lexically_normal().generic_string();
            texture->width = static_cast<std::uint32_t>(width);
            texture->height = static_cast<std::uint32_t>(height);
            texture->channels = 4u;
            texture->sourceFormat = TextureSourceFormat::EXR;
            texture->rgba32fPixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);

            free(pixels);
            return texture;
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
        texture->width = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(width, 0));
        texture->height = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(height, 0));
        texture->channels = 4;
        texture->sourceFormat = TextureLoaderDetail::DetectFormat(path);
        texture->rgba8Pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (std::size_t i = 3u; i < texture->rgba8Pixels.size(); i += 4u)
        {
            const std::uint8_t alpha = texture->rgba8Pixels[i];
            if (alpha < 250u)
            {
                texture->hasTransparentPixels = true;
            }
            if (alpha > 5u && alpha < 250u)
            {
                texture->hasPartialAlphaPixels = true;
            }
        }

        stbi_image_free(pixels);
        return texture;
    }

    std::shared_ptr<Texture> TextureLoader::LoadRGBA32F(const std::filesystem::path &path)
    {
        std::vector<std::uint8_t> fileData;
        try
        {
            fileData = Platform::FileSystem::ReadBinaryFile(path.string());
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_WARN("Failed to read HDR texture '{}': {}", path.string(), error.what());
            return {};
        }

        const TextureSourceFormat sourceFormat = TextureLoaderDetail::DetectFormat(path);
        if (sourceFormat == TextureSourceFormat::EXR)
        {
            return TextureLoaderDetail::LoadEXR32F(path, fileData);
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        float *pixels = stbi_loadf_from_memory(fileData.data(),
                                               static_cast<int>(fileData.size()),
                                               &width,
                                               &height,
                                               &channels,
                                               4);
        if (pixels == nullptr)
        {
            PHYSARA_CORE_WARN("Failed to load HDR texture '{}'.", path.string());
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->path = path.lexically_normal().generic_string();
        texture->width = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(width, 0));
        texture->height = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(height, 0));
        texture->channels = 4u;
        texture->sourceFormat = sourceFormat;
        texture->rgba32fPixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);

        stbi_image_free(pixels);
        return texture;
    }
}