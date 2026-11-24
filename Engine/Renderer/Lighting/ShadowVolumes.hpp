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

    /// @brief 交付一盏局部灯的有限视图或未能构造它的原因. planes 使用相机相对坐标.
    [[nodiscard]] std::variant<LocalShadowVolume, ShadowPlanReason> MakeLocalShadowVolume(const ShadowReceiver &receiver,
        std::span<const CullingPlane, 6> planes, const LightRecord &light, double shadowDistance) noexcept;

    /// @brief 从冻结场景为局部灯选取可影响其阴影体的 caster, 交付完成的记录.
    [[nodiscard]] LocalShadowPlan CollectLocalShadowCasters(const SceneFrame &scene, LocalShadowVolume volume);

    /// @brief 按准入边长构造完整方向光组. 返回值持有逐级 caster 下标, 使用它们时须沿用 scene.
    /// 名义切分以米计, 拟合范围包含相邻层的过渡带. 构造失败时返回整组的拒绝原因.
    /// @note 投射物列表的分配失败由计划入口处理.
    [[nodiscard]] std::variant<DirectionalShadowPlan, ShadowPlanReason> MakeDirectionalShadowPlan(
        const SceneFrame &scene, const ShadowReceiver &receiver, const LightRecord &light,
        const ShadowSettings &settings, std::uint32_t edge);
}
