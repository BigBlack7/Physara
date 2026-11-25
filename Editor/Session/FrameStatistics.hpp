#pragma once

/// @file
/// @brief Runtime 面向编辑器发布的只读统计值. 借用只覆盖同一次 EditorSession::Advance.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <GraphicsStatistics.hpp>
#include <Renderer/Paths/Forward.hpp>
#include <Renderer/Prepare/ViewPreparation.hpp>
#include <Renderer/RenderGraph/GraphResourcePool.hpp>

namespace Physara::Editor
{
    /// @brief 当前提交的计时状态. Skipped 表示已有查询全在用, Unavailable 表示设备未提供可用查询.
    enum class GpuIntervalState { Unavailable, Skipped, Pending, Ready, Failed };

    /// @brief 标识一次正常场景的完整提交. 基础预览请求与实际阴影诊断各有标识.
    /// 路径, 显示设置和两个布局均来自已接受的颜色. 计时器按值保留这份身份, 查询完成时仍归原票据.
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
        std::uint64_t shaderGeneration{}; ///< 此票据实际使用的程序代, 后续准备轮可独立推进.
        std::uint32_t directionalCascades{}; ///< 当前颜色若为级联诊断则交付实际级数.
        std::optional<Renderer::LocalShadowSelection> localShadow; ///< 当前颜色查询的局部灯.
        Renderer::OpaquePath opaquePath = Renderer::OpaquePath::Forward; ///< 该票据实际接受的不透明策略.
    };

    /// @brief 互不重叠的主要 CPU 区间, 数值为秒. UI 包括编辑器推进和图前 UI 准备.
    /// scenePreparation 截止于候选构图前, graphCompile 只覆盖 CompileRenderGraph.
    /// graphDeclare 包含构图、资源声明及 Seal. 区间之和只表示采样到的 CPU 工作.
    struct FrameCpuSeconds
    {
        double ui{};
        double scenePreparation{};
        double graphCompile{};
        double graphExecute{};
        double graphDeclare{};
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
        /// 三项合计本图所有存活 View 的 Forward 写入. drawUploads 包含实例块和间接参数, 单位为笔和字节.
        ForwardUploadStatistics staticUploads, viewUploads, drawUploads;
        /// 封存前两路合计的原始需求. 各来源的写入只计最终计划中的存活写 Pass.
        ForwardUploadStatistics uploadDemand;
        FrameArenaStatistics payload, graph;
    };

    /// @brief 一路 View 最近一次正常接受的场景及其阴影工作. work 缺席表示该颜色没有消费阴影.
    struct AcceptedViewShadows
    {
        Renderer::ViewId view;
        Scene::SceneId scene;
        std::uint64_t submission;
        std::optional<Renderer::RenderedShadows> work;
    };

    /// @brief CPU 区间, 原生计数与存活 Pass 数属于整图. forward 的表面, 灯及两个几何 Pass 的计数属于 source.view.
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

    /// @brief 最近编译结果的有界摘要. 安全图与失败前缀可以有提交身份, 不覆盖正常场景快照.
    struct GraphFrameSummary
    {
        /// 图代来自编译器. submission 为零表示尚无接受票据, 非零时还须按 accepted 区分有效图与前缀.
        std::uint64_t generation{}, plannedBytes{}, submission{};
        std::size_t declared{}, live{}, imports{}, exports{}, slots{};
        std::uint64_t nativeTextures{}, nativeBuffers{}; ///< 此图执行区间内实际新建的原生存储数.
        bool safeUi{}, accepted{};
    };

    /// @brief 一路 View 最近完成的 CPU 阴影准备. 包含当前布局原因, 与场景接受票据无关.
    struct ViewShadowStatistics
    {
        Renderer::ViewId view;
        std::uint64_t layoutRevision; ///< 本次像素布局的请求代. 阴影设置的会话请求代另见 plan.requestRevision.
        std::uint64_t sceneCameraRevision; ///< 场景相机可独立于光源及几何内容换代.
        std::uint64_t documentRevision; ///< 所属 View 的文档代, 包含导航相机与相机引用.
        Renderer::ViewSettingReason viewReason;
        Renderer::ShadowPlanStatistics plan;
    };

    /// @brief 当前 Froxel 请求的副本. 上传失败时仍保留 CPU 规划和所属输入身份.
    struct ViewFroxelStatistics
    {
        Renderer::ViewId view;
        /// 三项分别标识布局请求, View 相机和 View 文档代. 场景内容和场景相机代保存在 plan.
        std::uint64_t layoutRevision{}, cameraRevision{}, documentRevision{};
        Renderer::ViewSettingReason viewReason;
        Renderer::FroxelStatus status;
        Renderer::FroxelPreparationStatistics plan;
        std::optional<Core::BoundedText> deliveryFailure;
    };

    /// @brief 场景, 计时, 图准入和灯光准备分别发布. 当前准备可先于首次场景提交出现.
    /// 读取 CPU 规划时按各路自身的 Scene 和 View 定位, latestScene 带此前被接受的提交身份.
    struct FrameStatistics
    {
        std::optional<SceneFrameStatistics> latestScene;
        std::optional<SceneGpuInterval> latestGpu;
        std::optional<GraphFrameSummary> latestGraph;
        Renderer::GraphPoolStatistics graphPool;
        std::optional<Core::BoundedText> graphPressure;
        Renderer::ShaderPreparationStatistics shader;
        Shader::ReloadStatistics shaderCpu;
        std::array<std::optional<ViewShadowStatistics>, 2> shadows; ///< 主 View 和本轮附加 View 的当前规划.
        std::array<std::optional<ViewFroxelStatistics>, Renderer::ForwardViewCount> froxels;
        /// 主 View 及最近选定的附加 View 各自的接受事实. 缺席表示该 View 尚未接受正常场景色.
        std::array<std::optional<AcceptedViewShadows>, Renderer::ForwardViewCount> acceptedShadows;
    };
}
