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
    inline constexpr std::uint32_t DfgEdge = 128; ///< 与 PrepareSheenResponse 的 texel 中心映射配对.
    inline constexpr std::uint32_t EnvironmentBakeVersion = 1; ///< 投影、颜色、卷积和解码工具版本统一随此代变更.
    inline constexpr std::uint32_t DfgBakeVersion = 2; ///< GGX 中心采样与 Charlie 端点采样的联合 profile.
    inline constexpr std::uint32_t SheenEnvironmentEdge = 64;
    inline constexpr std::uint32_t SheenEnvironmentMipCount = 5;
    inline constexpr std::uint32_t SheenEnvironmentBakeVersion = 1; ///< 固定 Charlie 卷积、采样及 mip 映射版本.
    inline constexpr double SheenMinPerceptualRoughness = 0.045; ///< 与 CoreSurface.glsl 的感知粗糙度下限配对.

    enum class EnvironmentCubeRole : std::uint8_t { Sky, Reflection, Sheen };

    using EnvironmentTexel = std::array<float, 4>;

    /// @brief 一个紧密 RGBA32F 的六面 mip. 辐亮度为有限值, alpha 固定为 1.
    struct CubeLevel
    {
        std::uint32_t edge;
        std::vector<EnvironmentTexel> pixels;

        /// @pre face 为 RHI cube 的 0..5 层, 本对象覆盖完整的 edge×edge 像素.
        [[nodiscard]] std::span<const EnvironmentTexel> Face(std::uint32_t face) const noexcept;
    };

    /// @brief 天空和 GGX 镜面 mip 0 共用基础层. Charlie 各层均为独立卷积.
    /// sh 按 1, y, z, x, xy, yz, 3z²-1, zx, x²-y² 排列, 已含余弦卷积与 Lambert 的 1/pi.
    struct BakedEnvironment
    {
        std::shared_ptr<const CubeLevel> base;
        std::array<std::shared_ptr<const CubeLevel>, EnvironmentMipCount - 1> prefiltered;
        std::array<std::shared_ptr<const CubeLevel>, SheenEnvironmentMipCount> sheen;
        std::array<EnvironmentTexel, 9> sh{};
        bool lighting = false;

        /// @pre role 及 mip 属于本产品已经烘焙完毕的范围.
        [[nodiscard]] const CubeLevel &Level(EnvironmentCubeRole role, std::uint32_t mip) const noexcept;
    };

    /// @brief RGBA16F 方向表. R=B, G=A+B 使用均匀中心; B=Charlie/Neubelt 使用平方 NoV 和 p 的端点网格.
    struct DfgTable
    {
        std::vector<std::array<std::uint16_t, 4>> pixels;
    };

    /// @brief 由固定 CPU 源烘焙实际需要的角色, 复用同键现有产品中的基础层和完整卷积.
    /// @return 完整只读产品. 合作取消返回空, 积分或分配失败由作业边界转换成诊断.
    [[nodiscard]] std::shared_ptr<const BakedEnvironment> BakeEnvironment(
        const Asset::EnvironmentImage &source, bool lighting, bool sheen,
        const std::shared_ptr<const BakedEnvironment> &existing, std::stop_token stop);
    /// @brief 分别在 GGX 与 Charlie 的网格积分并量化 DFG. 全表就绪后才交付, 取消时返回空.
    [[nodiscard]] std::shared_ptr<const DfgTable> BakeDfg(std::stop_token stop);
}
