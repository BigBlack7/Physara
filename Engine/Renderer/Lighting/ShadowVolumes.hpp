#pragma once

/// @file
/// @brief Lighting 内部共享的有限接收范围及投射物筛选入口.
/// 局部灯按其有限体积筛选. 方向光在接收切片与发射轴上拟合, 随每级交付独立的世界投影.

#include "ShadowPlan.hpp"

namespace Physara::Renderer
{
    struct LightRecord;
}

namespace Physara::Renderer::Detail
{
    /// @brief 用于拟合和缓存匹配的稳定相机几何. worldFromView 是列主序刚体变换.
    /// projectionScale 保存稳定投影的 X/Y 尺度, sideLimit 是含采样余量的 NDC 正侧边界.
    struct ShadowReceiver
    {
        std::array<double, 16> worldFromView;
        std::array<double, 2> projectionScale;
        std::array<double, 2> sideLimit;
        double nearPlane;
        double farPlane;
        bool orthographic;
        bool operator==(const ShadowReceiver &) const = default;
    };

    /// @brief 根据灯的米制范围和接收视锥构造有限灯体. planes 使用相机相对坐标.
    /// 成功后由 MakeLocalShadowPlan 按候选层边长编码投影, 此时尚未占用区域.
    [[nodiscard]] std::variant<LocalShadowVolume, ShadowPlanReason> MakeLocalShadowVolume(const ShadowReceiver &receiver,
        std::span<const CullingPlane, 6> planes, const LightRecord &light, double shadowDistance) noexcept;

    /// @brief 在交付精度下拟合局部灯投影, 并从同一冻结场景选取 caster.
    /// @return 横向投影或写入深度不可用时返回空, 该组不会占用层.
    /// @param edge 候选布局的完整层边长, 用于 caster 偏移及点光的一 texel 跨面边带.
    [[nodiscard]] std::optional<LocalShadowPlan> MakeLocalShadowPlan(const SceneFrame &scene, LocalShadowVolume volume,
                                                                    std::uint32_t edge);

    /// @brief 按准入边长构造完整方向光组. 返回值持有逐级 caster 下标, 使用它们时须沿用 scene.
    /// 名义切分以米计, 拟合范围包含相邻层的过渡带. 构造失败时返回整组的拒绝原因.
    /// @note 投射物列表的分配失败由计划入口处理.
    [[nodiscard]] std::variant<DirectionalShadowPlan, ShadowPlanReason> MakeDirectionalShadowPlan(
        const SceneFrame &scene, const ShadowReceiver &receiver, const LightRecord &light,
        const ShadowSettings &settings, std::uint32_t edge);
}
