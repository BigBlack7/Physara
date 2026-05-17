#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Physara::Engine
{
    enum class TextureSourceFormat
    {
        Unknown,
        PNG,
        JPG,
        EXR
    };

    struct Texture
    {
        std::string path{};
        std::uint32_t width{0};
        std::uint32_t height{0};
        std::uint32_t channels{0};
        bool hasTransparentPixels{false};
        bool hasPartialAlphaPixels{false};
        TextureSourceFormat sourceFormat{TextureSourceFormat::Unknown};
        std::vector<std::uint8_t> rgba8Pixels{};
        std::vector<float> rgba32fPixels{};

        [[nodiscard]] bool IsLoaded() const
        {
            return width > 0 && height > 0;
        }
    };
}