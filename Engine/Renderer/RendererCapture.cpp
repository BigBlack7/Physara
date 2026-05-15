#include "RendererCapture.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include <stb/stb_image_write.h>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::Engine
{
    namespace RendererCaptureDetail
    {
        static void FlipRowsRGBA8(std::vector<std::uint8_t> &pixels, std::uint32_t width, std::uint32_t height)
        {
            const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
            std::vector<std::uint8_t> row(rowBytes);

            for (std::uint32_t y = 0; y < height / 2u; ++y)
            {
                std::uint8_t *top = pixels.data() + static_cast<std::size_t>(y) * rowBytes;
                std::uint8_t *bottom = pixels.data() + static_cast<std::size_t>(height - 1u - y) * rowBytes;
                std::copy(top, top + rowBytes, row.data());
                std::copy(bottom, bottom + rowBytes, top);
                std::copy(row.begin(), row.end(), bottom);
            }
        }

        static std::vector<std::uint8_t> ResizeRGBA8(
            const std::vector<std::uint8_t> &src,
            std::uint32_t srcWidth,
            std::uint32_t srcHeight,
            std::uint32_t dstWidth,
            std::uint32_t dstHeight)
        {
            if (srcWidth == dstWidth && srcHeight == dstHeight)
            {
                return src;
            }

            std::vector<std::uint8_t> dst(static_cast<std::size_t>(dstWidth) * dstHeight * 4u);
            for (std::uint32_t y = 0; y < dstHeight; ++y)
            {
                const float v = dstHeight > 1u
                                    ? static_cast<float>(y) * static_cast<float>(srcHeight - 1u) / static_cast<float>(dstHeight - 1u)
                                    : 0.f;
                const auto y0 = static_cast<std::uint32_t>(std::floor(v));
                const auto y1 = std::min(y0 + 1u, srcHeight - 1u);
                const float fy = v - static_cast<float>(y0);

                for (std::uint32_t x = 0; x < dstWidth; ++x)
                {
                    const float u = dstWidth > 1u
                                        ? static_cast<float>(x) * static_cast<float>(srcWidth - 1u) / static_cast<float>(dstWidth - 1u)
                                        : 0.f;
                    const auto x0 = static_cast<std::uint32_t>(std::floor(u));
                    const auto x1 = std::min(x0 + 1u, srcWidth - 1u);
                    const float fx = u - static_cast<float>(x0);

                    const std::size_t dstIndex = (static_cast<std::size_t>(y) * dstWidth + x) * 4u;
                    const std::size_t p00 = (static_cast<std::size_t>(y0) * srcWidth + x0) * 4u;
                    const std::size_t p10 = (static_cast<std::size_t>(y0) * srcWidth + x1) * 4u;
                    const std::size_t p01 = (static_cast<std::size_t>(y1) * srcWidth + x0) * 4u;
                    const std::size_t p11 = (static_cast<std::size_t>(y1) * srcWidth + x1) * 4u;

                    for (std::uint32_t c = 0; c < 4u; ++c)
                    {
                        const float top = static_cast<float>(src[p00 + c]) * (1.f - fx) + static_cast<float>(src[p10 + c]) * fx;
                        const float bottom = static_cast<float>(src[p01 + c]) * (1.f - fx) + static_cast<float>(src[p11 + c]) * fx;
                        dst[dstIndex + c] = static_cast<std::uint8_t>(std::clamp(top * (1.f - fy) + bottom * fy, 0.f, 255.f));
                    }
                }
            }

            return dst;
        }
    }

    CaptureResult RendererCapture::CaptureTexture(RHI::RHICommandList &commandList, RHI::RHITexture &texture, const CaptureDesc &desc)
    {
        CaptureResult result{};
        result.outputPath = desc.outputPath;

        if (desc.outputPath.empty())
        {
            result.message = "Capture output path is empty.";
            return result;
        }

        if (desc.format == CaptureFormat::EXR)
        {
            result.message = "EXR capture is reserved until HDR output is stable.";
            return result;
        }

        const std::uint32_t width = texture.GetWidth();
        const std::uint32_t height = texture.GetHeight();
        if (width == 0 || height == 0)
        {
            result.message = "Capture source texture has zero size.";
            return result;
        }

        RHI::RHITextureReadbackDesc readback{};
        readback.width = width;
        readback.height = height;
        readback.format = RHI::TextureFormat::RGBA8;

        std::vector<std::uint8_t> pixels = commandList.ReadTextureToCPU(&texture, readback);
        if (pixels.empty())
        {
            result.message = "Texture readback returned no pixels.";
            return result;
        }

        RendererCaptureDetail::FlipRowsRGBA8(pixels, width, height);

        const float scale = std::clamp(desc.resolutionScale, 0.25f, 4.f);
        const auto outputWidth = std::max(1u, static_cast<std::uint32_t>(std::round(static_cast<float>(width) * scale)));
        const auto outputHeight = std::max(1u, static_cast<std::uint32_t>(std::round(static_cast<float>(height) * scale)));
        pixels = RendererCaptureDetail::ResizeRGBA8(pixels, width, height, outputWidth, outputHeight);

        std::error_code ec;
        std::filesystem::create_directories(desc.outputPath.parent_path(), ec);
        if (ec)
        {
            result.message = "Failed to create capture output directory: " + ec.message();
            return result;
        }

        int written = 0;
        const std::string outputPath = desc.outputPath.string();
        if (desc.format == CaptureFormat::PNG)
        {
            written = stbi_write_png(outputPath.c_str(),
                                     static_cast<int>(outputWidth),
                                     static_cast<int>(outputHeight),
                                     4,
                                     pixels.data(),
                                     static_cast<int>(outputWidth * 4u));
        }
        else if (desc.format == CaptureFormat::JPG)
        {
            written = stbi_write_jpg(outputPath.c_str(),
                                     static_cast<int>(outputWidth),
                                     static_cast<int>(outputHeight),
                                     4,
                                     pixels.data(),
                                     std::clamp(desc.jpgQuality, 1, 100));
        }

        if (written == 0)
        {
            result.message = "stb_image_write failed to write capture file.";
            return result;
        }

        if (desc.includeDebugView)
        {
            PHYSARA_CORE_INFO("Capture includeDebugView requested; current renderer output is used until debug passes are connected.");
        }
        if (!desc.usePostExposureOutput)
        {
            PHYSARA_CORE_INFO("Capture requested pre-exposure output; current SceneColor is already the final LDR output.");
        }

        result.success = true;
        result.message = "Capture written.";
        return result;
    }
}