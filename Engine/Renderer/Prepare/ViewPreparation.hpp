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

    /// @brief 准备失败时只交付身份与原因. 成功时 SceneFrame 和索引借用当前槽与该 View 的工作区.
    /// 相机快照按值持有同轮镜头和曝光; documentRevision 只表示 View 请求, 不代替 Scene 内容版本.
    struct PreparedView
    {
        ViewId view;
        std::uint64_t requestRevision = 0;
        std::uint64_t cameraRevision = 0;
        std::uint64_t documentRevision = 0; ///< 场景候选核对此代, 使只变化曝光的旧快照也不能继续确认.
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
        /// 多 View 准备期间保持 Scene 不变, 单次调用期间保持 View 请求不变, 以配对其版号和曝光.
        /// 无有效布局时不提取 Scene; 失败不交付矩阵或部分索引.
        [[nodiscard]] PreparedView Prepare(SceneFrameStorage &sceneStorage, const Scene::Scene &scene,
                                           const View &view, const std::optional<ViewportRequest> &request,
                                           const CameraSampling &sampling = {});

    private:
        std::vector<std::uint32_t> m_Primitives;
        std::vector<std::uint32_t> m_Lights;
    };
}
