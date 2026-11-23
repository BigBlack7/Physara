#pragma once

/// @file
/// @brief Lighting 内部共享的有限接收范围及投射物筛选入口.

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

    /// @brief 交付一盏灯的有限视图或未能构造它的原因. planes 使用相机相对坐标.
    /// 返回的方向光 fitted 先覆盖接收段, 收集投射物后才得到供消费者读取的深度区间.
    [[nodiscard]] std::variant<ShadowVolume, ShadowPlanReason> MakeShadowVolume(const ShadowReceiver &receiver,
        std::span<const CullingPlane, 6> planes, const LightRecord &light, double shadowDistance) noexcept;

    /// @brief 从冻结的全部 Primitive 选取候选, 并为方向光纳入 caster 所需的光空间深度.
    /// @pre casters 是新组的空列表. 筛选期间 volume 和列表都可能已写入部分结果.
    /// @return 方向光拟合范围无法表示时返回 false, 调用方放弃此组. 列表的分配失败由计划入口处理.
    [[nodiscard]] bool CollectShadowCasters(const SceneFrame &scene, ShadowVolume &volume,
                                           std::vector<std::uint32_t> &casters);
}
