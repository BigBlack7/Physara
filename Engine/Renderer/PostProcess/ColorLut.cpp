#include "ColorLut.hpp"

/// @file
/// @brief 在 CPU 生成设备共享的 ColorGrading 节点, 保持场景曝光作为 LUT 的输入.

#include <algorithm>
#include <cmath>

#include <FilamentDisplay.hpp>
#include <glm/gtc/packing.hpp>

namespace Physara::Renderer::PostProcess
{
    /// @brief 对均匀 LogC 网格求逆. 首节点严格为零, 末节点覆盖约 58.8566 的预曝光输入.
    static std::array<float, LutEdge> ShaperNodes() noexcept
    {
        constexpr double A = 5.555556;
        constexpr double B = 0.047996;
        constexpr double C = 0.244161;
        constexpr double D = 0.386036;
        const double black = C * std::log10(B) + D;
        std::array<float, LutEdge> nodes{};
        for (std::size_t index = 1; index < nodes.size(); ++index)
        {
            const double encoded = std::lerp(black, 1.0, double(index) / (nodes.size() - 1));
            nodes[index] = static_cast<float>((std::pow(10.0, (encoded - D) / C) - B) / A);
        }
        return nodes;
    }

    std::vector<LutTexel> BakeColorLut()
    {
        const auto nodes = ShaperNodes();
        std::vector<LutTexel> pixels(std::size_t(LutAtlasEdge) * LutAtlasEdge);
        for (std::uint32_t b = 0; b < LutEdge; ++b)
        {
            // 蓝轴相邻的两片可能跨 atlas 行. Shader 在每片内过滤 R/G, 再单独插值 B.
            const auto tileX = (b % LutTilesPerRow) * LutEdge;
            const auto tileY = (b / LutTilesPerRow) * LutEdge;
            for (std::uint32_t g = 0; g < LutEdge; ++g)
            {
                for (std::uint32_t r = 0; r < LutEdge; ++r)
                {
                    const auto rgb = FilamentDisplay::GradeSdr({nodes[r], nodes[g], nodes[b]});
                    if (!std::isfinite(rgb.r) || !std::isfinite(rgb.g) || !std::isfinite(rgb.b))
                    {
                        return {};
                    }
                    pixels[(tileY + g) * LutAtlasEdge + tileX + r] = {
                        glm::packHalf1x16(rgb.r), glm::packHalf1x16(rgb.g), glm::packHalf1x16(rgb.b),
                        glm::packHalf1x16(1.0f)};
                }
            }
        }
        return pixels;
    }
}
