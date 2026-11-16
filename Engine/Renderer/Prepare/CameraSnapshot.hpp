#pragma once

/// @file
/// @brief 准备相机、有效预览结果和输入映射共享的矩阵与视线合同.

#include <array>
#include <cstdint>
#include <optional>

#include <Scene/Scene.hpp>

namespace Physara::Renderer
{
    /// @brief 世界空间视线, origin 以米计, direction 为指向场景的单位向量.
    struct CameraRay
    {
        std::array<double, 3> origin;
        std::array<double, 3> direction;
    };

    /// @brief 本帧相机的独立值. 右手系, 列主序, 主视图 NDC 深度为 [0,1] Reverse-Z.
    /// stable 投影不含 jitter 且有有限远面, render 投影保留请求的远面语义与本次偏移.
    /// 场景身份与 View 请求版本由外层 PreparedView 携带, 本值不代表已提交的相机历史.
    struct CameraSnapshot
    {
        std::optional<Scene::EntityHandle> entity; ///< 空值表示 View 拥有的导航相机.
        std::uint64_t sceneCameraRevision;
        std::array<double, 16> worldFromView;
        std::array<double, 16> viewFromWorld;
        std::array<double, 16> stableClipFromView;
        std::array<double, 16> viewFromStableClip;
        std::array<double, 16> renderClipFromView;
        std::array<double, 16> viewFromRenderClip;
        std::array<double, 2> jitterPixels;
        double aspectRatio;
        double nearPlane;
        double cullingFar;
        std::optional<double> renderFar; ///< 缺省表示渲染使用无限远透视.
        bool orthographic;

        /// @brief 从有效图像的左上 UV 得到稳定视线, 两轴范围均为 [0,1). 越界或无法表示时返回空.
        /// 透视射线起于眼点, 正交射线起于对应近面位置; 两者都不含本帧 jitter.
        [[nodiscard]] std::optional<CameraRay> RayAt(double u, double v) const noexcept;
    };
}
