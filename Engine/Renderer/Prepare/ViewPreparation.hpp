#pragma once

/// @file
/// @brief 在图声明前解析相机、有效图像范围与每 View 的主可见集.
/// 矩阵按值交付, 静态记录和候选索引由安全帧槽保活; 此处不创建图或 GPU 资源.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "SceneFrame.hpp"
#include "../View.hpp"

namespace Physara::Renderer
{
    /// @brief 渲染像素中的采样位移及 profile 上界. 正方向为右、下.
    /// 各分量须有限, 上界和滤波半径非负, 实际位移的绝对值不得超过上界.
    /// 裁剪只使用上界与滤波半径, 不随实际 jitter 改变.
    struct CameraSampling
    {
        std::array<double, 2> jitterPixels{};
        std::array<double, 2> maxAbsJitterPixels{};
        std::array<double, 2> filterRadiusPixels{1, 1};
    };

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

    /// @brief 准备失败时只交付身份与原因. 成功时 SceneFrame 和索引借用当前槽与该 View 的工作区.
    struct PreparedView
    {
        ViewId view;
        std::uint64_t requestRevision = 0;
        std::uint64_t cameraRevision = 0;
        ViewSettingReason reason = ViewSettingReason::LayoutPending;
        std::optional<Core::BoundedText> failure;
        std::optional<CameraSnapshot> camera;
        ViewImageLayout layout;
        SceneFrame scene;
        std::span<const std::uint32_t> primitives;
        std::span<const std::uint32_t> lights;
    };

    /// @brief 一个安全槽中供单个 View 复用的 CPU 索引区间. 多 View 的区间各自独立.
    class ViewWork final
    {
    public:
        /// @brief 解析本轮布局与相机, 共享同槽的 SceneFrame, 写入自己的可见索引.
        /// @pre Scene 已接受本轮事务, sceneStorage 所属槽已安全取得.
        /// @note 可用同一 sceneStorage 和各自的 ViewWork 顺序准备多个 View. 本对象再次准备即结束旧索引借用.
        /// 多 View 准备期间保持 Scene 不变. 无有效布局时不提取 Scene; 失败不交付矩阵或部分索引.
        [[nodiscard]] PreparedView Prepare(SceneFrameStorage &sceneStorage, const Scene::Scene &scene,
                                           const View &view, const std::optional<ViewportRequest> &request,
                                           const CameraSampling &sampling = {});

    private:
        std::vector<std::uint32_t> m_Primitives;
        std::vector<std::uint32_t> m_Lights;
    };
}
