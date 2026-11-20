#pragma once

/// @file
/// @brief Runtime 面向编辑器发布的只读统计值. 借用只覆盖同一次 EditorSession::Advance.

#include <array>
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
        /// 两个版本分别来自场景静态内容和本条记录对应的 View 文档请求.
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

    /// @brief 一路实际完成的裁剪及其输出确认, 暂停或无匹配布局不产生裁剪记录.
    struct ViewCullingStatistics
    {
        Renderer::ViewId view;
        double seconds{};
        std::size_t primitiveCandidates{}, visiblePrimitives{};
        std::size_t lightCandidates{}, visibleLights{};
        bool accepted{};
    };

    /// @brief 单个 arena 的容量和历史峰值以字节计, growths 累计该槽存续期间成功扩容的次数.
    struct FrameArenaStatistics
    {
        std::size_t capacity{}, highWater{}, growths{};
    };

    /// @brief 按对应阶段统计的 Forward 写入笔数与载荷字节数.
    struct ForwardUploadStatistics
    {
        std::size_t writes{}, bytes{};
    };

    /// @brief 一次共享提取及各 View 的裁剪. 上传按已执行的图写入分类, 原生总量在外层记录.
    struct ScenePreparationStatistics
    {
        std::size_t slot{};
        std::uint64_t structureRevision{};
        Renderer::SceneExtractionStatistics extraction;
        Renderer::ForwardEncodingCounts encodings;
        std::array<ViewCullingStatistics, Renderer::ForwardViewCount> views;
        ForwardUploadStatistics staticUploads, viewUploads;
        /// 封存前两路合计的原始需求. staticUploads 和 viewUploads 只计最终计划中的存活写 Pass.
        ForwardUploadStatistics uploadDemand;
        FrameArenaStatistics payload, graph;
    };

    /// @brief CPU 区间, 原生计数与存活 Pass 数属于整图. forward 的 Draw 和灯数属于 source.view.
    /// 两路都接受时选主视口为 source, 仅附加路接受时选该路. 每路裁剪与接受情况在 preparation.views 中.
    /// 仅尝试了窗口交换时才有交换耗时.
    struct SceneFrameStatistics
    {
        SceneFrameIdentity source;
        FrameCpuSeconds cpu;
        RHI::GraphicsOperationCounts native;
        RHI::GraphicsUploadStorage uploadStorage;
        Renderer::ForwardWorkCounts forward;
        Renderer::ForwardResidencyCounts residency;
        std::size_t livePasses;
        bool presented;
        std::optional<double> presentWaitSeconds;
        GpuIntervalState gpu;
        ScenePreparationStatistics preparation;
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
