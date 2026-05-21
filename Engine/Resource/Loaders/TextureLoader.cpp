#include "TextureLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>
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

        struct EXRHeaderGuard
        {
            EXRHeader header{};

            EXRHeaderGuard()
            {
                InitEXRHeader(&header);
            }

            ~EXRHeaderGuard()
            {
                FreeEXRHeader(&header);
            }
        };

        struct EXRImageGuard
        {
            EXRImage image{};

            EXRImageGuard()
            {
                InitEXRImage(&image);
            }

            ~EXRImageGuard()
            {
                FreeEXRImage(&image);
            }
        };

        int FindEXRChannel(const EXRHeader &header, std::string_view semanticName)
        {
            int layeredFallback = -1;
            for (int i = 0; i < header.num_channels; ++i)
            {
                const std::string_view channelName(header.channels[i].name);
                if (channelName == semanticName)
                {
                    return i;
                }

                if (channelName.size() > semanticName.size() + 1u &&
                    channelName.ends_with(semanticName) &&
                    channelName[channelName.size() - semanticName.size() - 1u] == '.')
                {
                    layeredFallback = layeredFallback < 0 ? i : layeredFallback;
                }
            }

            return layeredFallback;
        }

        std::string DescribeEXRChannels(const EXRHeader &header)
        {
            std::ostringstream stream;
            for (int i = 0; i < header.num_channels; ++i)
            {
                if (i > 0)
                {
                    stream << ", ";
                }
                stream << header.channels[i].name;
            }
            return stream.str();
        }

        float SanitizeHDRValue(float value, std::size_t &invalidCount)
        {
            if (!std::isfinite(value))
            {
                ++invalidCount;
                return 0.f;
            }

            return std::max(value, 0.f);
        }

        std::shared_ptr<Texture> LoadEXR32F(const std::filesystem::path &path, const std::vector<std::uint8_t> &fileData)
        {
            const char *error = nullptr;
            EXRVersion version{};
            int result = ParseEXRVersionFromMemory(&version, fileData.data(), fileData.size());
            if (result != TINYEXR_SUCCESS)
            {
                PHYSARA_CORE_WARN("Failed to parse EXR version '{}': code {}.", path.string(), result);
                return {};
            }

            if (version.multipart || version.non_image)
            {
                PHYSARA_CORE_WARN("Unsupported EXR layout '{}': multipart={}, non_image={}.",
                                  path.string(),
                                  version.multipart,
                                  version.non_image);
                return {};
            }

            EXRHeaderGuard headerGuard{};
            result = ParseEXRHeaderFromMemory(&headerGuard.header, &version, fileData.data(), fileData.size(), &error);
            if (result != TINYEXR_SUCCESS)
            {
                PHYSARA_CORE_WARN("Failed to parse EXR header '{}': {}",
                                  path.string(),
                                  error != nullptr ? error : "Unknown EXR header error");
                if (error != nullptr)
                {
                    FreeEXRErrorMessage(error);
                }
                return {};
            }

            for (int i = 0; i < headerGuard.header.num_channels; ++i)
            {
                if (headerGuard.header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
                {
                    headerGuard.header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
                }
            }

            const int redChannel = FindEXRChannel(headerGuard.header, "R");
            const int greenChannel = FindEXRChannel(headerGuard.header, "G");
            const int blueChannel = FindEXRChannel(headerGuard.header, "B");
            const int alphaChannel = FindEXRChannel(headerGuard.header, "A");
            const bool grayscale = headerGuard.header.num_channels == 1;
            if (!grayscale && (redChannel < 0 || greenChannel < 0 || blueChannel < 0))
            {
                PHYSARA_CORE_WARN("EXR '{}' does not expose RGB channels. Channels: [{}].",
                                  path.string(),
                                  DescribeEXRChannels(headerGuard.header));
                return {};
            }

            const auto isFloatReadable = [&header = headerGuard.header](int channel)
            {
                return channel < 0 ||
                       header.pixel_types[channel] == TINYEXR_PIXELTYPE_HALF ||
                       header.pixel_types[channel] == TINYEXR_PIXELTYPE_FLOAT;
            };
            if (!isFloatReadable(grayscale ? 0 : redChannel) ||
                !isFloatReadable(grayscale ? 0 : greenChannel) ||
                !isFloatReadable(grayscale ? 0 : blueChannel) ||
                !isFloatReadable(alphaChannel))
            {
                PHYSARA_CORE_WARN("EXR '{}' contains non-floating point RGB(A) channels. Channels: [{}].",
                                  path.string(),
                                  DescribeEXRChannels(headerGuard.header));
                return {};
            }

            EXRImageGuard imageGuard{};
            error = nullptr;
            result = LoadEXRImageFromMemory(&imageGuard.image, &headerGuard.header, fileData.data(), fileData.size(), &error);
            if (result != TINYEXR_SUCCESS)
            {
                PHYSARA_CORE_WARN("Failed to load EXR pixels '{}': {}",
                                  path.string(),
                                  error != nullptr ? error : "Unknown EXR image error");
                if (error != nullptr)
                {
                    FreeEXRErrorMessage(error);
                }
                return {};
            }

            if (imageGuard.image.width <= 0 || imageGuard.image.height <= 0)
            {
                PHYSARA_CORE_WARN("EXR '{}' decoded to an empty image.", path.string());
                return {};
            }

            auto texture = std::make_shared<Texture>();
            texture->path = path.lexically_normal().generic_string();
            texture->width = static_cast<std::uint32_t>(imageGuard.image.width);
            texture->height = static_cast<std::uint32_t>(imageGuard.image.height);
            texture->channels = 4u;
            texture->sourceFormat = TextureSourceFormat::EXR;
            texture->rgba32fPixels.assign(static_cast<std::size_t>(texture->width) * texture->height * 4u, 0.f);

            std::size_t invalidCount = 0u;
            float maxValue = 0.f;
            const auto writePixel = [&](std::size_t destinationIndex, const float *const *channels, std::size_t sourceIndex)
            {
                const int r = grayscale ? 0 : redChannel;
                const int g = grayscale ? 0 : greenChannel;
                const int b = grayscale ? 0 : blueChannel;
                const float red = SanitizeHDRValue(channels[r][sourceIndex], invalidCount);
                const float green = SanitizeHDRValue(channels[g][sourceIndex], invalidCount);
                const float blue = SanitizeHDRValue(channels[b][sourceIndex], invalidCount);
                const float alpha = alphaChannel >= 0 ? SanitizeHDRValue(channels[alphaChannel][sourceIndex], invalidCount) : 1.f;

                const std::size_t base = destinationIndex * 4u;
                texture->rgba32fPixels[base + 0u] = red;
                texture->rgba32fPixels[base + 1u] = green;
                texture->rgba32fPixels[base + 2u] = blue;
                texture->rgba32fPixels[base + 3u] = alpha;
                maxValue = std::max(maxValue, std::max(red, std::max(green, blue)));
            };

            if (imageGuard.image.images != nullptr)
            {
                const auto channels = reinterpret_cast<const float *const *>(imageGuard.image.images);
                const std::size_t pixelCount = static_cast<std::size_t>(texture->width) * texture->height;
                for (std::size_t i = 0u; i < pixelCount; ++i)
                {
                    writePixel(i, channels, i);
                }
            }
            else if (imageGuard.image.tiles != nullptr)
            {
                for (int tileIndex = 0; tileIndex < imageGuard.image.num_tiles; ++tileIndex)
                {
                    const EXRTile &tile = imageGuard.image.tiles[tileIndex];
                    if (tile.images == nullptr || tile.width <= 0 || tile.height <= 0)
                    {
                        continue;
                    }

                    const auto channels = reinterpret_cast<const float *const *>(tile.images);
                    const int sourceStride = headerGuard.header.tile_size_x;
                    for (int y = 0; y < tile.height; ++y)
                    {
                        for (int x = 0; x < tile.width; ++x)
                        {
                            const int dstX = tile.offset_x * headerGuard.header.tile_size_x + x;
                            const int dstY = tile.offset_y * headerGuard.header.tile_size_y + y;
                            if (dstX < 0 || dstY < 0 || dstX >= imageGuard.image.width || dstY >= imageGuard.image.height)
                            {
                                continue;
                            }

                            const std::size_t destinationIndex =
                                static_cast<std::size_t>(dstY) * texture->width + static_cast<std::size_t>(dstX);
                            const std::size_t sourceIndex =
                                static_cast<std::size_t>(y) * static_cast<std::size_t>(sourceStride) + static_cast<std::size_t>(x);
                            writePixel(destinationIndex, channels, sourceIndex);
                        }
                    }
                }
            }
            else
            {
                PHYSARA_CORE_WARN("EXR '{}' produced neither scanline nor tiled image data.", path.string());
                return {};
            }

            PHYSARA_CORE_INFO("Loaded EXR '{}': {}x{}, tiled={}, channels=[{}], maxRGB={}, sanitizedInvalid={}.",
                              path.string(),
                              texture->width,
                              texture->height,
                              headerGuard.header.tiled != 0,
                              DescribeEXRChannels(headerGuard.header),
                              maxValue,
                              invalidCount);
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