#pragma once

/// @file
/// @brief 在图声明前解析相机和像素范围, 从同一份冻结场景交付主可见集及 CPU 阴影计划.
/// 矩阵按值交付, 安全帧槽保活静态记录, 可见索引和本轮借用的计划.

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

    /// @brief 主准备失败时交付身份与原因. 成功时 SceneFrame 和索引借用当前槽与该 View 的工作区.
    /// 相机快照按值持有同轮镜头和曝光; documentRevision 只表示 View 请求, 不代替 Scene 内容版本.
    /// 阴影单独报告成功或拒绝. 主准备成功时即使 shadows 为空, 相机和直接光输入仍可消费.
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
        double cullingSeconds{}; ///< 主可见几何和灯的筛选用时. 阴影拟合在计时结束后运行.
        const ShadowPlan *shadows{}; ///< 与 scene 同轮的 CPU 计划, 借用持有它的 ViewWork.
        ShadowPlanStatistics shadowStatistics; ///< 规划不可用时仍保留本轮身份及原因.
        ShadowDiagnostic shadowDiagnostic; ///< 冻结的会话请求, 可与实际准入结果不同.
        const ShadowLightPlan *previewShadow{}; ///< 借用本次计划, 仅供具备实际可见几何的诊断使用.
        std::optional<ShadowDepthSet> acceptedShadowDepth; ///< 此 View 原本接受的存储, Forward 对照实际消耗确认可重用性.

        /// @brief 有效诊断只返回选中组. 普通受光返回全部准入组, 其余分支返回空区间.
        /// @note 返回值借用此轮 ViewWork 持有的计划, 调用者在工作区再次准备前消费.
        [[nodiscard]] std::span<const ShadowLightPlan> ShadowGroups(bool lighting) const noexcept
        {
            if (previewShadow)
            {
                return std::span(previewShadow, 1);
            }
            return lighting && shadows ? std::span<const ShadowLightPlan>(shadows->admitted) :
                                         std::span<const ShadowLightPlan>{};
        }
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

    /// @brief 一个安全槽中供单个 View 复用的主可见索引和阴影计划借用. 多 View 的工作区各自独立.
    /// View 保存跨槽缓存, 此处另持本次计划的只读引用. 其他槽推进缓存时仍可读取本槽的旧计划.
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
                                           ViewportUse use = ViewportUse::DisplayOutput,
                                           const ShadowPlanningLimits &shadowLimits = {});
        /// @brief 同帧的各路共用一个 SceneFramePreparation, 每路使用独立 ViewWork 保存索引.
        /// @pre 全部请求准备完毕前保持 Scene 和本槽的提取数组稳定.
        /// 显式离屏的两种像素范围都通过上限检查后, 才会请求共享提取.
        /// @param requireShadowPreview 普通显示附件超出预算时, 只有可消费的阴影诊断能使布局通过审核.
        [[nodiscard]] PreparedView Prepare(SceneFramePreparation &scene, const View &view,
                                           const std::optional<ViewportRequest> &request,
                                           const CameraSampling &sampling = {},
                                           ViewportUse use = ViewportUse::DisplayOutput,
                                           const ShadowPlanningLimits &shadowLimits = {},
                                           bool requireShadowPreview = false);

        /// @brief 布局或附件预算拒绝时清理该 View 的活动分配, 并以当前场景版本发布空计划摘要.
        /// 释放数取自上一次交付的活动区域. 调用方负责关联到所属 View 的布局请求.
        /// @note 本工作区的 shadows 借用止于再次 Prepare, 调用本函数或工作区销毁.
        [[nodiscard]] ShadowPlanStatistics UnavailableShadows(const Scene::Scene &scene, const View &view,
                                                            const ShadowPlanningLimits &limits) noexcept;

    private:
        std::vector<std::uint32_t> m_Primitives;
        std::vector<std::uint32_t> m_Lights;
        std::shared_ptr<const ShadowPlan> m_Shadows;
    };
}
