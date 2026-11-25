#pragma once

/// @file
/// @brief 交付单个 View 的 CPU Froxel 网格、压紧灯行和对应的稳定坐标查询.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <StaticForward/Data.hpp>

namespace Physara::Renderer
{
    enum class FroxelStatus : std::uint8_t;
    struct FroxelRequest;
    struct PreparedView;

    /// @brief 本轮 ViewWork 拥有的完整载荷. 方向灯仍由前缀读取.
    /// headers 按 X 最快, 其次 Y, 最后 Z 排列. 每个 [offset, offset + count) 以 indices 条目为单位.
    /// indices 与 globalLights 保存同一完整灯表的绝对行号. 查询一个 cell 时还需读取方向灯前缀及全局局部灯.
    struct FroxelLightLists
    {
        Shader::Generated::StaticForward::FroxelView grid{};
        std::vector<Shader::Generated::StaticForward::FroxelCellHeader> headers;
        std::vector<Shader::Generated::StaticForward::FroxelLightIndex> indices;
        std::vector<Shader::Generated::StaticForward::FroxelLightIndex> globalLights;
    };

    /// @brief 从渲染画布的左上像素坐标和米制正向视深定位 cell.
    /// @pre grid 来自同一个成功准备的工作集. renderPixel 包含有效矩形的画布原点及当帧抖动.
    /// @return 所属 header 的下标. 内部边界归入后一格, 有限远面归入最后一层. 区间外或非有限样点返回空.
    [[nodiscard]] std::optional<std::uint32_t> QueryFroxelCell(
        const Shader::Generated::StaticForward::FroxelView &grid,
        std::array<float, 2> renderPixel, float viewDepth) noexcept;

    namespace Detail
    {
        /// @brief 准备当前 View 的完整载荷或返回不可用原因.
        /// @pre view 已成功准备且借用有效. sideLimit 是同轮稳定视锥的两轴 NDC 外缘.
        /// @return 成功时移交整份 CPU 数组的所有权. 失败时临时数组随本次构造释放.
        [[nodiscard]] std::variant<std::unique_ptr<FroxelLightLists>, FroxelStatus> BuildFroxelLightLists(
            const PreparedView &view, const FroxelRequest &request, std::array<double, 2> sideLimit);
    }
}
