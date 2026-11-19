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
        PreviewMode preview = PreviewMode::BaseColor; ///< 本次准备时的会话预览模式, 供绘制和合成共用.
        ViewSettingReason reason = ViewSettingReason::LayoutPending;
        std::optional<Core::BoundedText> failure;
        std::optional<CameraSnapshot> camera;
        AntiAliasing requestedAa = AntiAliasing::None; ///< 随本轮文档代封存, 供后处理选择.
        ViewLayouts layouts; ///< 同一相机的两次像素适配. 裁剪和抖动使用 render, 场景显示与命中使用 display.
        SceneFrame scene;
        std::span<const std::uint32_t> primitives;
        std::span<const std::uint32_t> lights;
        double cullingSeconds{};
    };

    /// @brief 一个正常候选的共享 SceneFrame. 首个合法 View 才提取, 后续 View 共享成功值或失败.
    /// @note 槽已安全取得, Scene 及提取存储在本轮 View 准备结束前保持有效.
    class SceneFramePreparation final
    {
    public:
        SceneFramePreparation(SceneFrameStorage &storage, const Scene::Scene &scene) noexcept
            : m_Storage(storage), m_Scene(scene) {}

        [[nodiscard]] const Scene::Scene &Source() const noexcept { return m_Scene; }
        /// @brief 首次调用取得本槽快照并保存结果, 同一轮的其余请求也会看到同一失败原因或记录.
        [[nodiscard]] const std::variant<SceneFrame, Core::BoundedText> &Extract();
        /// @brief 读取本轮已提取的快照. 未提取或失败时为空, 不触发新准备.
        [[nodiscard]] const SceneFrame *Result() const noexcept;

    private:
        SceneFrameStorage &m_Storage;
        const Scene::Scene &m_Scene;
        std::optional<std::variant<SceneFrame, Core::BoundedText>> m_Extracted;
    };

    /// @brief 一个安全槽中供单个 View 复用的 CPU 索引区间. 多 View 的区间各自独立.
    class ViewWork final
    {
    public:
        /// @brief 为单个请求打开一次提取作用域, 根据布局和相机写入自己的可见索引.
        /// @pre Scene 已接受本轮事务, sceneStorage 所属槽已安全取得.
        /// @note 本对象再次准备即结束旧索引借用. 调用期间保持 View 请求不变, 以配对其版号和曝光.
        /// 无有效布局时不提取 Scene; 失败不交付矩阵或部分索引.
        [[nodiscard]] PreparedView Prepare(SceneFrameStorage &sceneStorage, const Scene::Scene &scene,
                                           const View &view, const std::optional<ViewportRequest> &request,
                                           const CameraSampling &sampling = {},
                                           ViewportUse use = ViewportUse::DisplayOutput);
        /// @brief 同帧的各路共用一个 SceneFramePreparation, 每路使用独立 ViewWork 保存索引.
        /// @pre 全部请求准备完毕前保持 Scene 和本槽的提取数组稳定.
        /// 显式离屏的两种像素范围都通过上限检查后, 才会请求共享提取.
        [[nodiscard]] PreparedView Prepare(SceneFramePreparation &scene, const View &view,
                                           const std::optional<ViewportRequest> &request,
                                           const CameraSampling &sampling = {},
                                           ViewportUse use = ViewportUse::DisplayOutput);

    private:
        std::vector<std::uint32_t> m_Primitives;
        std::vector<std::uint32_t> m_Lights;
    };
}
