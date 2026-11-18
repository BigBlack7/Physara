#include "EnvironmentDecode.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

#include <tinyexr.h>

/// @file
/// @brief 约束 EXR 的解码形状、色域和临时内存, 将 planar 数据转成拥有额度的线性辐亮度.

namespace Physara::Asset
{
    /// @brief 解析失败也可能交回部分对象或消息. 由本次 worker 解码在同一作用域收拢.
    struct ExrDecode final
    {
        EXRHeader header{};
        EXRImage image{};
        const char *error = nullptr;

        ~ExrDecode()
        {
            FreeEXRImage(&image);
            FreeEXRHeader(&header);
            if (error)
            {
                FreeEXRErrorMessage(error);
            }
        }
    };

    /// @brief 发布后的借用指向 pixels. 最后一个读者释放字节后归还保有额度.
    struct EnvironmentPixels final
    {
        std::shared_ptr<ByteBudget> budget;
        std::unique_ptr<float[]> pixels;
        std::size_t reserved = 0;

        ~EnvironmentPixels()
        {
            pixels.reset();
            budget->Release(reserved);
        }
    };

    /// @brief 复制解码器借用的诊断到自持边界.
    static AssetFailure DecodeFailure(AssetError error, std::string_view reason) noexcept
    {
        return {error, Core::BoundedText(reason)};
    }

    /// @brief 固定 scanline profile 的每块行数. 返回零表示该编码不经过此入口.
    static std::size_t ScanlinesPerChunk(int compression) noexcept
    {
        switch (compression)
        {
        case TINYEXR_COMPRESSIONTYPE_NONE:
        case TINYEXR_COMPRESSIONTYPE_ZIPS:
            return 1;
        case TINYEXR_COMPRESSIONTYPE_ZIP:
            return 16;
        case TINYEXR_COMPRESSIONTYPE_PIZ:
            return 32;
        default:
            return 0;
        }
    }

    /// @brief 输入为亮度域数据. 有明确色域声明时核对所支持的场景线性 Rec.709.
    static std::optional<AssetFailure> ReadColorMetadata(const EXRHeader &header, std::optional<float> &white)
    {
        constexpr std::array rec709{0.6400f, 0.3300f, 0.3000f, 0.6000f, 0.1500f, 0.0600f, 0.3127f, 0.3290f};
        for (int index = 0; index != header.num_custom_attributes; ++index)
        {
            const auto &attribute = header.custom_attributes[index];
            const std::string_view name(attribute.name);
            const std::string_view type(attribute.type);
            if (name == "chromaticities")
            {
                std::array<float, 8> values{};
                if (type != "chromaticities" || attribute.size != static_cast<int>(sizeof(values)))
                {
                    return DecodeFailure(AssetError::InvalidImage, "Invalid EXR chromaticities metadata");
                }
                std::memcpy(values.data(), attribute.value, sizeof(values));
                if (!std::ranges::equal(values, rec709,
                    [](float value, float expected) { return std::abs(value - expected) <= 1.e-4f; }))
                {
                    return DecodeFailure(AssetError::UnsupportedEncoding, "EXR chromaticities must be Rec.709/D65");
                }
            }
            else if (name == "colorInteropID")
            {
                if (type != "string" || attribute.size <= 0)
                {
                    return DecodeFailure(AssetError::InvalidImage, "Invalid EXR colorInteropID metadata");
                }
                const std::string_view color(reinterpret_cast<const char *>(attribute.value),
                                             static_cast<std::size_t>(attribute.size));
                if (color != "lin_rec709_scene" && color != "unknown")
                {
                    return DecodeFailure(AssetError::UnsupportedEncoding, "Unsupported EXR colorInteropID");
                }
            }
            else if (name == "acesImageContainerFlag")
            {
                std::int32_t flag = 0;
                if (type != "int" || attribute.size != static_cast<int>(sizeof(flag)))
                {
                    return DecodeFailure(AssetError::InvalidImage, "Invalid EXR ACES container metadata");
                }
                std::memcpy(&flag, attribute.value, sizeof(flag));
                if (flag != 0)
                {
                    return DecodeFailure(AssetError::UnsupportedEncoding,
                                         "ACES metadata conflicts with the Rec.709 environment profile");
                }
            }
            else if (name == "whiteLuminance")
            {
                float value = 0;
                if (type != "float" || attribute.size != static_cast<int>(sizeof(value)))
                {
                    return DecodeFailure(AssetError::InvalidImage, "Invalid EXR whiteLuminance metadata");
                }
                std::memcpy(&value, attribute.value, sizeof(value));
                if (!std::isfinite(value) || value <= 0)
                {
                    return DecodeFailure(AssetError::InvalidImage, "EXR whiteLuminance must be positive and finite");
                }
                white = value;
            }
        }
        return std::nullopt;
    }

    std::variant<EnvironmentImage, AssetFailure>
    DecodeEnvironment(std::span<const std::byte> bytes, std::size_t imageLimit,
                      const std::shared_ptr<ByteBudget> &budget)
    {
        const auto *input = reinterpret_cast<const unsigned char *>(bytes.data());
        EXRVersion version{};
        if (ParseEXRVersionFromMemory(&version, input, bytes.size()) != TINYEXR_SUCCESS)
        {
            return DecodeFailure(AssetError::UnsupportedEncoding, "Environment source must be OpenEXR");
        }
        if (version.multipart || version.non_image || version.tiled)
        {
            return DecodeFailure(AssetError::UnsupportedEncoding, "Environment EXR must be single-part scanline");
        }
        ExrDecode decode;
        auto &header = decode.header;
        if (ParseEXRHeaderFromMemory(&header, &version, input, bytes.size(), &decode.error) != TINYEXR_SUCCESS)
        {
            return DecodeFailure(AssetError::InvalidImage, decode.error ? decode.error : "Cannot parse EXR header");
        }
        const auto linesPerChunk = ScanlinesPerChunk(header.compression_type);
        if (header.tiled || header.line_order < 0 || header.line_order > 1 || linesPerChunk == 0)
        {
            return DecodeFailure(AssetError::UnsupportedEncoding, "Unsupported EXR scanline layout or compression");
        }

        std::array<int, 4> channels{-1, -1, -1, -1};
        if (header.num_channels < 3 || header.num_channels > 4)
        {
            return DecodeFailure(AssetError::UnsupportedEncoding, "Environment EXR requires RGB or RGBA");
        }
        for (int index = 0; index != header.num_channels; ++index)
        {
            const auto &channel = header.channels[index];
            constexpr std::string_view names("RGBA");
            const std::string_view name(channel.name);
            const auto component = name.size() == 1 ? names.find(name) : std::string_view::npos;
            if (component == std::string_view::npos || channels[component] >= 0 ||
                channel.x_sampling != 1 || channel.y_sampling != 1 ||
                (channel.pixel_type != TINYEXR_PIXELTYPE_HALF && channel.pixel_type != TINYEXR_PIXELTYPE_FLOAT))
            {
                return DecodeFailure(AssetError::UnsupportedEncoding, "Unsupported EXR channel, type or sampling");
            }
            channels[component] = index;
            header.requested_pixel_types[index] = TINYEXR_PIXELTYPE_FLOAT;
        }
        if (channels[0] < 0 || channels[1] < 0 || channels[2] < 0)
        {
            return DecodeFailure(AssetError::UnsupportedEncoding, "Environment EXR requires all three RGB channels");
        }

        const auto &window = header.data_window;
        const auto width = std::int64_t(window.max_x) - window.min_x + 1;
        const auto height = std::int64_t(window.max_y) - window.min_y + 1;
        if (height <= 0 || width != 2 * height)
        {
            return DecodeFailure(AssetError::InvalidImage, "Environment EXR data window must have a 2:1 aspect");
        }
        if (width > std::numeric_limits<int>::max())
        {
            return DecodeFailure(AssetError::ImageByteLimit, "Environment EXR exceeds the decoded byte limit");
        }
        // 固定版 TinyEXR 按非零 chunkCount 分配偏移表. 允许其使用从窗口和编码推导的唯一块数.
        const auto rows = static_cast<std::size_t>(height);
        const auto chunkCount = (rows + linesPerChunk - 1) / linesPerChunk;
        if (header.chunk_count < 0 ||
            (header.chunk_count != 0 && static_cast<std::size_t>(header.chunk_count) != chunkCount))
        {
            return DecodeFailure(AssetError::InvalidImage, "EXR chunkCount does not match the scanline layout");
        }
        constexpr std::size_t offsetSize = sizeof(std::uint64_t);
        if (chunkCount > imageLimit / offsetSize)
        {
            return DecodeFailure(AssetError::ImageByteLimit, "Environment EXR offset table exceeds the decoded byte limit");
        }
        const auto offsetBytes = chunkCount * offsetSize;
        // magic/version 共八字节, 已由 ParseEXRVersionFromMemory 核对. 偏移表紧接 header.
        const auto available = bytes.size() - 8;
        if (header.header_len > available || offsetBytes > available - header.header_len)
        {
            return DecodeFailure(AssetError::InvalidImage, "Truncated EXR scanline offset table");
        }
        // 在给解码器分配前对偏移表、平面和紧密 RGB 的字节统一准入.
        const auto floatsPerPixel = static_cast<std::size_t>(header.num_channels) + 3;
        const auto pixelLimit = (imageLimit - offsetBytes) / (floatsPerPixel * sizeof(float));
        if (static_cast<std::size_t>(width) > pixelLimit / rows)
        {
            return DecodeFailure(AssetError::ImageByteLimit, "Environment EXR exceeds the decoded byte limit");
        }
        const auto count = std::size_t(width) * rows;
        const auto retained = count * 3 * sizeof(float);
        std::optional<float> white;
        if (const auto failure = ReadColorMetadata(header, white))
        {
            return *failure;
        }
        auto storage = std::make_shared<EnvironmentPixels>();
        storage->budget = budget;
        if (!budget->TryReserve(retained))
        {
            return DecodeFailure(AssetError::PixelCapacity, "Environment pixels exceed the retained budget");
        }
        storage->reserved = retained;
        if (LoadEXRImageFromMemory(&decode.image, &header, input, bytes.size(), &decode.error) != TINYEXR_SUCCESS)
        {
            return DecodeFailure(AssetError::InvalidImage, decode.error ? decode.error : "Cannot decode EXR pixels");
        }
        storage->pixels = std::make_unique_for_overwrite<float[]>(count * 3);
        for (std::size_t component = 0; component < 3; ++component)
        {
            const auto *plane = reinterpret_cast<const float *>(decode.image.images[channels[component]]);
            for (std::size_t y = 0; y < rows; ++y)
            {
                // 固定版 TinyEXR 按 dataWindow 的 y 坐标放置平面, 源块顺序不改变逻辑行序.
                const auto row = y * std::size_t(width);
                for (std::size_t x = 0; x < std::size_t(width); ++x)
                {
                    const float value = plane[row + x];
                    if (!std::isfinite(value))
                    {
                        return DecodeFailure(AssetError::InvalidImage, "EXR RGB radiance must be finite");
                    }
                    storage->pixels[(row + x) * 3 + component] = std::max(0.0f, value);
                }
            }
        }
        return EnvironmentImage{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
            std::shared_ptr<const float[]>(storage, storage->pixels.get()),
            {EnvironmentEncoding::OpenExr, white ? RadianceCalibration::Absolute : RadianceCalibration::Relative},
            white};
    }
}
