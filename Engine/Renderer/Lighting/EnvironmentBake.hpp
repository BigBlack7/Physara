#pragma once

/// @file
/// @brief 环境冷路径的只读产物. 像素保持源辐亮度域, 并按 RHI 立方体面和 mip 顺序交付.

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

#include <Asset/AssetManager.hpp>

namespace Physara::Renderer::Detail
{
    inline constexpr std::uint32_t EnvironmentEdge = 256;
    inline constexpr std::uint32_t EnvironmentMipCount = 5;
    inline constexpr std::uint32_t DfgEdge = 128;
    inline constexpr std::uint32_t EnvironmentBakeVersion = 1; ///< 投影、颜色、卷积和解码工具版本统一随此代变更.
    inline constexpr std::uint32_t DfgBakeVersion = 1; ///< Filament 固定模型、坐标和精度的版本.

    using EnvironmentTexel = std::array<float, 4>;

    /// @brief 一个紧密 RGBA32F 的六面 mip. 辐亮度为有限值, alpha 固定为 1.
    struct CubeLevel
    {
        std::uint32_t edge;
        std::vector<EnvironmentTexel> pixels;

        /// @pre face 为 RHI cube 的 0..5 层, 本对象覆盖完整的 edge×edge 像素.
        [[nodiscard]] std::span<const EnvironmentTexel> Face(std::uint32_t face) const noexcept;
    };

    /// @brief 天空和镜面 mip 0 共用基础层. lighting 为真时其余四层为 GGX 卷积结果.
    /// sh 按 1, y, z, x, xy, yz, 3z²-1, zx, x²-y² 排列, 已含余弦卷积与 Lambert 的 1/pi.
    struct BakedEnvironment
    {
        std::shared_ptr<const CubeLevel> base;
        std::array<std::shared_ptr<const CubeLevel>, EnvironmentMipCount - 1> prefiltered;
        std::array<EnvironmentTexel, 9> sh{};
        bool lighting = false;
    };

    /// @brief 按 NoV 列和 p 递增行保存 RGBA16F, R=B, G=A+B, B=cloth.
    struct DfgTable
    {
        std::vector<std::array<std::uint16_t, 4>> pixels;
    };

    /// @brief 由固定 CPU 源烘焙实际需要的角色. base 可借助同键已有产品共享.
    /// @return 完整只读产品. 合作取消返回空, 积分或分配失败由作业边界转换成诊断.
    [[nodiscard]] std::shared_ptr<const BakedEnvironment> BakeEnvironment(
        const Asset::EnvironmentImage &source, bool lighting, std::shared_ptr<const CubeLevel> base,
        std::stop_token stop);
    /// @brief 在 texel 中心积分并量化共享 DFG. 全表就绪后才交付, 取消时返回空.
    [[nodiscard]] std::shared_ptr<const DfgTable> BakeDfg(std::stop_token stop);
}
