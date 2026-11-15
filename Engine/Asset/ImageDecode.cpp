#include "ImageDecode.hpp"

/// @file
/// @brief 固定 stb 的 PNG/JPEG 内存解码适配, 以及像素独立的颜色和数据用途说明.

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

namespace Physara::Asset
{
    struct DecodeWorkspace;
    static thread_local DecodeWorkspace *ActiveWorkspace = nullptr;

    /// @brief 在本次解码中限制 stb 单次 realloc 的目标大小, 由输入与已验证的像素布局推导上限.
    /// 这是临时增长缓冲区上限, 与 PixelBudget 对最终像素的跨版本记账分开.
    struct DecodeWorkspace final
    {
        const std::size_t limit;
        bool exhausted = false;

        explicit DecodeWorkspace(std::size_t maxBlock) noexcept : limit(maxBlock)
        {
            assert(!ActiveWorkspace);
            ActiveWorkspace = this;
        }

        DecodeWorkspace(const DecodeWorkspace &) = delete;
        DecodeWorkspace &operator=(const DecodeWorkspace &) = delete;
        ~DecodeWorkspace() { ActiveWorkspace = nullptr; }
    };

    /// @brief 保留 realloc 的失败语义. PNG 中间流展开过大时拒绝增长, 由 stb 释放原缓冲区.
    static void *GrowDecodeBuffer(void *buffer, std::size_t bytes) noexcept
    {
        if (ActiveWorkspace && bytes > ActiveWorkspace->limit)
        {
            ActiveWorkspace->exhausted = true;
            return nullptr;
        }
        return std::realloc(buffer, bytes);
    }
}

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
// 固定 stb 会用静态缓冲区拼接未知 PNG 关键块的名字. 关闭库诊断, 由适配层返回自持错误.
#define STBI_NO_FAILURE_STRINGS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MALLOC(bytes) std::malloc(bytes)
#define STBI_REALLOC(buffer, bytes) Physara::Asset::GrowDecodeBuffer(buffer, bytes)
#define STBI_FREE(buffer) std::free(buffer)
#include <stb_image.h>

namespace Physara::Asset
{
    bool PixelBudget::TryReserve(std::size_t bytes) noexcept
    {
        // 这里只同步额度计数. 像素和完成数据的可见性由作业交接提供.
        auto used = m_Used.load(std::memory_order_relaxed);
        do
        {
            if (bytes > m_Limit - used)
            {
                return false;
            }
        } while (!m_Used.compare_exchange_weak(used, used + bytes, std::memory_order_relaxed));
        return true;
    }

    void PixelBudget::Release(std::size_t bytes) noexcept
    {
        [[maybe_unused]] const auto previous = m_Used.fetch_sub(bytes, std::memory_order_relaxed);
        assert(previous >= bytes);
    }

    /// @brief C 库像素及其额度的唯一所有者. 共享像素指针借用此对象的引用计数.
    struct DecodedPixels final
    {
        unsigned char *data = nullptr;
        std::shared_ptr<PixelBudget> budget;
        std::size_t bytes = 0;

        ~DecodedPixels()
        {
            stbi_image_free(data);
            if (budget)
            {
                budget->Release(bytes);
            }
        }
    };

    static constexpr std::array<unsigned char, 8> PngSignature{0x89, 'P', 'N', 'G', 13, 10, 26, 10};

    /// @brief 由文件签名识别编码, 与路径后缀无关.
    static std::optional<ImageEncoding> Identify(std::span<const unsigned char> bytes) noexcept
    {
        if (bytes.size() >= PngSignature.size() && std::ranges::equal(bytes.first(PngSignature.size()), PngSignature))
        {
            return ImageEncoding::Png;
        }
        if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8)
        {
            return ImageEncoding::Jpeg;
        }
        return std::nullopt;
    }

    /// @brief 沿真实块边界查找 stb 可识别的 CgBI. 该变体不能直接交付直通 RGBA.
    /// @pre 已识别 PNG 签名. 块内容与像素校验仍由解码器执行.
    /// 只比较块类型, 不把元数据中的同名文字当成编码; 截断结构返回 InvalidImage.
    static std::optional<AssetFailure> CheckPngProfile(std::span<const unsigned char> bytes) noexcept
    {
        constexpr std::size_t ChunkOverhead = 12; // 长度、类型和 CRC.
        bytes = bytes.subspan(PngSignature.size());
        // stb 在 IDAT 后仍可识别 CgBI, 因此扫描到 IEND, 不能在首个像素块处提前放行.
        while (bytes.size() >= ChunkOverhead)
        {
            const auto length = (static_cast<std::size_t>(bytes[0]) << 24) |
                (static_cast<std::size_t>(bytes[1]) << 16) | (static_cast<std::size_t>(bytes[2]) << 8) | bytes[3];
            if (length > bytes.size() - ChunkOverhead)
            {
                break;
            }

            const std::string_view type(reinterpret_cast<const char *>(bytes.data() + 4), 4);
            if (type == "CgBI")
            {
                return AssetFailure{AssetError::UnsupportedEncoding, Core::BoundedText("CgBI PNG is not supported")};
            }
            if (type == "IEND")
            {
                return std::nullopt;
            }
            bytes = bytes.subspan(ChunkOverhead + length);
        }

        return AssetFailure{AssetError::InvalidImage, Core::BoundedText("PNG has an incomplete chunk sequence")};
    }

    /// @brief JPEG 头探测失败时, 从 SOF0/1/2 的精度字段确认是否超出 8 位 profile.
    /// @pre 输入带 JPEG SOI. 仅识别位深异常, 其余结构校验和尺寸以 stb 探测结果为准.
    static bool HasNon8BitJpegFrame(std::span<const unsigned char> bytes) noexcept
    {
        bytes = bytes.subspan(2);
        while (!bytes.empty())
        {
            const auto prefix = std::ranges::find(bytes, 0xff);
            const auto marker = std::find_if(prefix, bytes.end(), [](unsigned char value)
            {
                return value != 0xff;
            });
            if (marker == bytes.end())
            {
                return false;
            }

            const auto type = *marker;
            bytes = bytes.subspan(static_cast<std::size_t>(marker - bytes.begin()) + 1);
            // 跳过带长度的头段. 到达扫描数据、结束或无长度标记时停止寻找帧头.
            if (type <= 0x01 || (type >= 0xd0 && type <= 0xda) || bytes.size() < 2)
            {
                return false;
            }
            const auto length = (static_cast<std::size_t>(bytes[0]) << 8) | bytes[1];
            if (length < 2 || length > bytes.size())
            {
                return false;
            }
            if (type >= 0xc0 && type <= 0xc2)
            {
                constexpr std::size_t MinimumFrameLength = 11; // 含长度字段及至少一个分量描述.
                return length >= MinimumFrameLength && bytes[2] != 8;
            }
            bytes = bytes.subspan(length);
        }
        return false;
    }

    /// @brief 校验输出尺寸并防止相乘溢出, 得到含四个通道的字节数.
    static std::optional<std::size_t> PixelBytes(int width, int height, std::size_t limit) noexcept
    {
        if (width <= 0 || height <= 0 ||
            static_cast<std::size_t>(width) > limit / 4 / static_cast<std::size_t>(height))
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(width) * 4 * static_cast<std::size_t>(height);
    }

    std::variant<Rgba8Image, AssetFailure>
    DecodeImage(std::span<const std::byte> bytes, std::size_t imageLimit, const std::shared_ptr<PixelBudget> &budget)
    {
        // 先按实际字节选择格式并查询头, 在像素分配前拒绝位深和不可表示的输出尺寸.
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return AssetFailure{AssetError::SourceByteLimit, Core::BoundedText("Image source exceeds decoder range")};
        }
        const auto *raw = reinterpret_cast<const unsigned char *>(bytes.data());
        const std::span<const unsigned char> input(raw, bytes.size());
        const auto encoding = Identify(input);
        const int length = static_cast<int>(bytes.size());
        if (!encoding)
        {
            return AssetFailure{AssetError::UnsupportedEncoding,
                                Core::BoundedText("Expected PNG samples up to 8 bits or an 8-bit JPEG")};
        }

        if (*encoding == ImageEncoding::Png)
        {
            if (const auto failure = CheckPngProfile(input))
            {
                return *failure;
            }
            if (stbi_is_16_bit_from_memory(raw, length))
            {
                return AssetFailure{AssetError::UnsupportedEncoding,
                                    Core::BoundedText("PNG sample depth exceeds 8 bits")};
            }
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        if (!stbi_info_from_memory(raw, length, &width, &height, &channels))
        {
            // 关闭库错误文本后只补充位深分类, 不把此轻量扫描当成另一套 JPEG 解码成功判据.
            if (*encoding == ImageEncoding::Jpeg && HasNon8BitJpegFrame(input))
            {
                return AssetFailure{AssetError::UnsupportedEncoding, Core::BoundedText("JPEG precision is not 8 bit")};
            }
            return AssetFailure{AssetError::InvalidImage, Core::BoundedText("Cannot read the image header")};
        }
        constexpr auto DecoderByteLimit = static_cast<std::size_t>(std::numeric_limits<int>::max());
        const auto size = PixelBytes(width, height, std::min(imageLimit, DecoderByteLimit));
        if (!size)
        {
            return AssetFailure{AssetError::ImageByteLimit, Core::BoundedText("Image exceeds the decoded byte limit")};
        }

        // 将预留额度交给局部 RAII owner. 后续解码失败、取消或异常都会沿同一析构归还.
        auto pixels = std::make_shared<DecodedPixels>();
        if (!budget->TryReserve(*size))
        {
            return AssetFailure{AssetError::PixelCapacity, Core::BoundedText("CPU pixel budget is occupied")};
        }
        pixels->budget = budget;
        pixels->bytes = *size;

        // 标准 PNG/JPEG 沿库默认顶部行序输出. CgBI 已在分配前拒绝, 无需额外通道或 alpha 转换.
        int decodedWidth = 0;
        int decodedHeight = 0;
        // 留出行标记、交错行及库按倍数扩容的空间. 输入和输出均已小于等于 int 上限.
        DecodeWorkspace workspace(2 * std::max(*size + static_cast<std::size_t>(height), bytes.size()) + 16384);
        pixels->data = stbi_load_from_memory(raw, length, &decodedWidth, &decodedHeight, &channels, STBI_rgb_alpha);
        if (workspace.exhausted)
        {
            return AssetFailure{AssetError::ImageByteLimit, Core::BoundedText("Decoder workspace exceeds its limit")};
        }
        if (!pixels->data || decodedWidth != width || decodedHeight != height)
        {
            return AssetFailure{AssetError::InvalidImage, Core::BoundedText("Cannot decode a complete RGBA8 image")};
        }

        // 别名共享指针暴露只读数组, 控制块仍拥有 C 内存和额度, 最后一个读者负责退休.
        const auto *data = pixels->data;
        return Rgba8Image{*encoding, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                         channels == 2 || channels == 4, *size, {std::move(pixels), data}};
    }

    std::optional<ImageSample> DescribeImage(ImageVersion image, ImageUse use) noexcept
    {
        if (!image)
        {
            return std::nullopt;
        }
        switch (use)
        {
        case ImageUse::BaseColor:
        case ImageUse::EditorColor:
        {
            const auto alpha = image->image.hasSourceAlpha ? AlphaMeaning::StraightCoverage : AlphaMeaning::Opaque;
            return ImageSample{std::move(image), use, ImageDomain::Srgb, alpha};
        }
        case ImageUse::Emissive:
            return ImageSample{std::move(image), use, ImageDomain::Srgb, AlphaMeaning::Ignored};
        case ImageUse::Normal:
        case ImageUse::OcclusionRoughnessMetallic:
            return ImageSample{std::move(image), use, ImageDomain::LinearData, AlphaMeaning::Ignored};
        case ImageUse::LinearData:
            return ImageSample{std::move(image), use, ImageDomain::LinearData, AlphaMeaning::Data};
        }
        return std::nullopt;
    }
}
