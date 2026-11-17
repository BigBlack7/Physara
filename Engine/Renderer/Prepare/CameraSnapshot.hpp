#pragma once

/// @file
/// @brief 准备相机, 有效预览结果和输入映射共享的矩阵, 光学与曝光值合同.
/// 快照按值封存同轮输入, 不借用可变 View; 是否已经成功渲染由外层输出记录表达.

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

    /// @brief 本轮使用的光学参数, 长度为米. yfov 为空表示正交相机.
    /// 焦距为空表示正交或场景透视的等效焦距无法表示, 场景投影仍按其自身参数求值.
    struct CameraLens
    {
        double fNumber;
        double sensorHeightMeters;
        double focusDistanceMeters;
        std::optional<double> yfov; ///< 透视垂直视场, 弧度.
        std::optional<double> focalLengthMeters;
    };

    /// @brief 未补偿名义 EV100 及应用补偿后的线性预曝光. 倍率按 GPU 使用的 FP32 值保存.
    /// View 求值保证倍率为正有限正规数, 帧准备及 Shader 打包不再从光圈/快门重复计算.
    struct CameraExposure
    {
        double ev100;
        float preExposure;
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
        CameraLens lens;
        CameraExposure exposure;

        /// @brief 从有效图像的左上 UV 得到稳定视线, 两轴范围均为 [0,1). 越界或无法表示时返回空.
        /// 透视射线起于眼点, 正交射线起于对应近面位置; 两者都不含本帧 jitter.
        [[nodiscard]] std::optional<CameraRay> RayAt(double u, double v) const noexcept;
    };
}
