#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

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