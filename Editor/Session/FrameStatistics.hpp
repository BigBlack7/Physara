#pragma once

/// @file
/// @brief Runtime 面向编辑器发布的只读统计值. 借用只覆盖同一次 EditorSession::Advance.

#include <cstddef>
#include <cstdint>
#include <optional>

#include <GraphicsStatistics.hpp>
#include <Renderer/Paths/Forward.hpp>

namespace Physara::Editor
{
    /// @brief 当前提交的计时状态. Skipped 表示已有查询全在用, Unavailable 表示设备未提供可用查询.
    enum class GpuIntervalState { Unavailable, Skipped, Pending, Ready, Failed };

    /// @brief 标识一次正常场景的完整提交, 所采用的范围与模式冻结在同一记录.
    struct SceneFrameIdentity
    {
        std::uint64_t submission;
        Scene::SceneId scene;
        Renderer::ViewId view;
        /// 两个版本分别来自场景静态内容和主 View 文档请求.
        std::uint64_t contentRevision;
        std::uint64_t viewRevision;
        Renderer::PreviewMode preview;
        /// 渲染和显示布局各含整张画布尺寸及相机在其中实际使用的像素矩形.
        Renderer::ViewImageLayout render;
        Renderer::ViewImageLayout displayLayout;
        Renderer::DisplaySettings display;
    };

    /// @brief 互不重叠的主要 CPU 区间, 数值为秒. UI 包括编辑器推进和图前 UI 准备.
    struct FrameCpuSeconds
    {
        double ui{};
        double scenePreparation{};
        double graphCompile{};
        double graphExecute{};
    };

    /// @brief 同一正常场景图的 CPU 区间, 原生调用和准备结果. 仅尝试了窗口交换时才有交换耗时.
    struct SceneFrameStatistics
    {
        SceneFrameIdentity source;
        FrameCpuSeconds cpu;
        RHI::GraphicsOperationCounts native;
        Renderer::ForwardWorkCounts forward;
        Renderer::ForwardResidencyCounts residency;
        std::size_t livePasses;
        bool presented;
        std::optional<double> presentWaitSeconds;
        GpuIntervalState gpu;
    };

    /// @brief GPU 区间由后续帧观察, 始终附带当时的图身份, 响应延迟不计入区间.
    struct SceneGpuInterval
    {
        SceneFrameIdentity source;
        double seconds;
    };

    /// @brief 分别保留最近出图与最近就绪计时. 消费者可按各自的 submission 判定时长属于哪次提交.
    struct FrameStatistics
    {
        std::optional<SceneFrameStatistics> latestScene;
        std::optional<SceneGpuInterval> latestGpu;
    };
}
