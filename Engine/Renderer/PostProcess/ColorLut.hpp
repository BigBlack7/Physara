#pragma once

/// @file
/// @brief 固定线性 SDR 显示查表的 CPU 像素与二维 atlas 约定.

#include <array>
#include <cstdint>
#include <vector>

namespace Physara::Renderer::PostProcess
{
    inline constexpr std::uint32_t LutEdge = 64;
    inline constexpr std::uint32_t LutTilesPerRow = 8;
    inline constexpr std::uint32_t LutAtlasEdge = LutEdge * LutTilesPerRow;

    using LutTexel = std::array<std::uint16_t, 4>;

    /// @brief 返回紧密排列的 RGBA16F atlas. 片内向右递增 R, 向下递增 G, B 每 8 个切片换一行.
    /// @return 自持的显示线性 RGB 和常量 alpha 1; 节点非有限时返回空数组, 分配失败向调用方传播.
    [[nodiscard]] std::vector<LutTexel> BakeColorLut();
}
