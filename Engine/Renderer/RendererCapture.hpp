#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Physara::RHI
{
    class RHICommandList;
    class RHITexture;
}

namespace Physara::Engine
{
    enum class CaptureFormat : std::uint8_t
    {
        PNG = 0,
        JPG,
        EXR
    };

    [[nodiscard]] inline constexpr std::string_view GetCaptureFormatExtension(CaptureFormat format)
    {
        switch (format)
        {
        case CaptureFormat::JPG:
            return ".jpg";
        case CaptureFormat::EXR:
            return ".exr";
        case CaptureFormat::PNG:
        default:
            return ".png";
        }
    }

    struct CaptureDesc
    {
        std::filesystem::path outputPath{};
        CaptureFormat format{CaptureFormat::PNG};
        float resolutionScale{1.f};
        bool includeDebugView{false};
        bool usePostExposureOutput{true};
        int jpgQuality{95};
    };

    struct CaptureResult
    {
        bool success{false};
        std::filesystem::path outputPath{};
        std::string message{};
    };

    class RendererCapture final
    {
    public:
        static CaptureResult CaptureTexture(RHI::RHICommandList &commandList, RHI::RHITexture &texture, const CaptureDesc &desc);
    };
}