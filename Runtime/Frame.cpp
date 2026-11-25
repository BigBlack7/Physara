#include "Frame.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <charconv>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <Renderer/RenderGraph/GraphRanges.hpp>

/// @file
/// @brief 分别完成候选准备, 图声明/编译和原列表执行, 在明确边界选择恢复或退出.
/// 失败尝试销毁后才重建图; 一旦 Open, 只处理原前缀而不追加恢复命令.
/// Scene/View 在候选循环前准备一次, 图尝试只消费准备结果及各 owner 已持有的依赖.

namespace Physara::Runtime
{
    using CpuClock = std::chrono::steady_clock;

    /// @brief 将同一次 CPU 阶段的单调时钟差换成统计快照使用的秒数.
    static double SecondsSince(CpuClock::time_point start) noexcept
    {
        return std::chrono::duration<double>(CpuClock::now() - start).count();
    }

    using namespace Renderer;

    static constexpr std::size_t MainViewIndex = 0;
    static constexpr std::size_t OffscreenViewIndex = 1;

    /// @brief 复制本次 CPU 规划的值摘要. 布局、Scene 相机及 View 文档版本也标记准备拒绝的来源.
    /// 快照可以跨图尝试保留, 不借用槽中的计划和可见索引.
    static Editor::ViewShadowStatistics MeasureShadows(const PreparedView &prepared,
                                                       std::uint64_t sceneCameraRevision) noexcept
    {
        return {prepared.view, prepared.requestRevision, sceneCameraRevision, prepared.documentRevision,
                prepared.reason, prepared.shadowStatistics};
    }

    void CollectSceneChanges(Editor::EditorSession &editor, std::span<FrameSlot, FrameSlotCount> slots) noexcept
    {
        const auto *scene = editor.ActiveScene();
        assert(scene);
        const auto changes = scene->PendingRenderChanges();
        for (auto &slot : slots)
        {
            slot.sceneFrame.Observe(changes);
        }
        // 当前在途的槽也已保留自己的待消费索引, Scene 此时才可清除本次分发的明细.
        editor.AcknowledgeSceneRenderChanges();
    }

    Platform::DeviceHost::PresentationAcquire AcquireFrame(Platform::DeviceHost &host, std::size_t &next)
    {
        for (std::size_t offset = 0; offset < FrameSlotCount; ++offset)
        {
            const auto index = (next + offset) % FrameSlotCount;
            auto acquired = host.BeginPresentation(index);
            if (std::holds_alternative<Platform::PresentationFrame>(acquired))
            {
                next = (index + 1) % FrameSlotCount;
                return acquired;
            }
            const auto *status = std::get_if<Platform::PresentationStatus>(&acquired);
            if (!status || *status != Platform::PresentationStatus::Retry)
            {
                return acquired;
            }
        }
        return Platform::PresentationStatus::Retry;
    }

    // 票据只关联已接受工作, 与后续交换及实际完成独立.
    static void ReportTicket(Core::Diagnostics &diagnostics, std::string_view event, std::uint64_t ticket) noexcept
    {
        std::array<char, 24> number{};
        const auto converted = std::to_chars(number.data(), number.data() + number.size(), ticket);
        Core::BoundedText message(event);
        message.Append(", ticket=");
        message.Append({number.data(), static_cast<std::size_t>(converted.ptr - number.data())});
        static_cast<void>(diagnostics.Write(Core::Severity::Info, message.View()));
    }

    EditorGraphProfile::EditorGraphProfile(const Platform::DeviceHost &host, std::size_t sceneViews,
                                           bool diagnostic, std::uint64_t shadowBytes)
    {
        const bool scene = sceneViews != 0;
        const auto info = host.GetInfo();
        constexpr std::array formats{RHI::Format::SRGBA8_UNORM, RHI::Format::RGBA8_UNORM,
                                     RHI::Format::RGBA16_FLOAT, RHI::Format::D32};
        auto count = scene ? formats.size() : 2;
        for (std::size_t index = 0; index < count; ++index)
        {
            m_Textures[index] = {.format = formats[index], .dimension = RHI::TextureDimension::Texture2D,
                .sampleCount = 1, .maxWidth = info.maxTextureSizePixels, .maxHeight = info.maxTextureSizePixels,
                .maxDepth = 1, .maxLayers = 1,
                .formatSupport = host.GetDevice()->QueryFormatSupport(formats[index]),
                .copySource = true, .copyDestination = true, .clear = true, .maxSampledLayers = 1,
                .presentation = formats[index] == RHI::Format::SRGBA8_UNORM};
            if (host.HasFatalGraphicsError())
            {
                return;
            }
        }
        if (scene)
        {
            std::uint32_t maxEdge = 0;
            constexpr auto format = RHI::Format::RGBA32_FLOAT;
            const auto support = host.QueryCubeFormatSupport(format, maxEdge);
            if ((support & RHI::FormatSupport::ShaderSample) != 0)
            {
                m_Textures[count++] = {.format = format, .dimension = RHI::TextureDimension::TextureCube,
                    .sampleCount = 1, .maxWidth = maxEdge, .maxHeight = maxEdge, .maxDepth = 1, .maxLayers = 6,
                    .formatSupport = support, .copySource = true, .copyDestination = true, .maxSampledLayers = 6};
            }
        }
        if (shadowBytes != 0)
        {
            m_States.back() = Renderer::Detail::SampledState(TextureAspect::Depth);
            std::uint32_t edge{}, layers{};
            if (host.QueryDepthArrayLimits(ShadowDepthFormat, edge, layers))
            {
                // 按实际数组目标的深度写入及采样能力准入. 下游绑定覆盖本 View 的连续层.
                m_Textures[count++] = {.format = ShadowDepthFormat, .dimension = RHI::TextureDimension::Texture2DArray,
                    .sampleCount = 1, .maxWidth = edge, .maxHeight = edge, .maxDepth = 1, .maxLayers = layers,
                    .formatSupport = RHI::FormatSupport::Texture | RHI::FormatSupport::DepthStencil |
                        RHI::FormatSupport::ShaderLoad | RHI::FormatSupport::ShaderSample,
                    .copySource = true, .copyDestination = true, .clear = true,
                    .maxSampledLayers = std::min(layers, MaxShadowLayers),
                    .layeredReadExport = true};
            }
        }
        const bool structuredUav = scene && host.SupportsDirectCompute();
        if (structuredUav)
        {
            const auto addStorage = [&](RHI::Format format, RHI::TextureDimension dimension)
            {
                std::uint32_t maxLayers = 0;
                const auto support = host.QueryStorageImageFormatSupport(format, dimension, maxLayers);
                if ((support & RHI::FormatSupport::ShaderUavStore) != RHI::FormatSupport::None)
                {
                    // 逐 target 记录同一设备观察到的能力. 允许创建 storage 的目标才进入场景编译表.
                    m_Textures[count++] = {.format = format, .dimension = dimension, .sampleCount = 1,
                        .maxWidth = info.maxTextureSizePixels, .maxHeight = info.maxTextureSizePixels,
                        .maxDepth = 1, .maxLayers = maxLayers, .formatSupport = support,
                        .copySource = true, .copyDestination = true,
                        .clear = bool(support & RHI::FormatSupport::RenderTarget),
                        .maxSampledLayers = (support & RHI::FormatSupport::ShaderSample) != RHI::FormatSupport::None ? 1u : 0u};
                }
            };
            // RGBA8 和 RGBA16F 的 2D 行已有实际格式位, 补充其它二维格式及数组目标.
            addStorage(RHI::Format::R32_FLOAT, RHI::TextureDimension::Texture2D);
            addStorage(RHI::Format::R32_UINT, RHI::TextureDimension::Texture2D);
            for (const auto format : {RHI::Format::R32_FLOAT, RHI::Format::R32_UINT,
                                      RHI::Format::RGBA8_UNORM, RHI::Format::RGBA16_FLOAT})
            {
                addStorage(format, RHI::TextureDimension::Texture2DArray);
            }
        }
        m_Options = {.textures = std::span(m_Textures).first(count),
            .buffers = {.maxBytes = scene ? ForwardBufferLimit : 8 * 1024 * 1024,
                        .maxConstantBytes = RHI::ConstantBufferOffsetSizeAlignment,
                        .constantOffsetAlignment = info.uniformBufferOffsetAlignmentBytes,
                        .maxStructuredBytes = scene ? info.maxStorageBufferBlockBytes : 0,
                        .structuredOffsetAlignment = scene ? info.storageBufferOffsetAlignmentBytes : 0,
                        .structuredUav = structuredUav},
            .textureBoundaryStates = std::span(m_States).first(shadowBytes != 0 ? 3u : 2u),
            .bufferBoundaryStates = std::span(m_States).first<2>(), .maxColorAttachments = 1,
            .transientByteBudget = sceneViews * ForwardAttachmentBudget + 32 * 1024 * 1024 + 4 +
                shadowBytes + ((!scene || diagnostic) ? DiagnosticUploadBudget : 0),
            .transientObjectBudget = GraphResourcePool::DefaultLimit.objects};
    }

    /// @brief 一次尝试只返回一种动作. 开放列表前的图拒绝或视口准入失败可重建; 录制后的失败不追加安全图.
    enum class AttemptStatus { Succeeded, RebuildWithoutViewport, UploadPending, GraphRejected, Cancelled, Stopped };

    /// @brief 一张候选图的最终动作与拥有型失败摘要, 返回后不再依赖该图或准备值.
    /// error 保留该候选首错, additional 说明后续收拢为何失败; 票据另记在原帧槽中.
    struct AttemptResult
    {
        AttemptStatus status;
        std::optional<Core::BoundedText> error;
        std::optional<Core::BoundedText> additional;
        ViewSettingReason viewportReason = ViewSettingReason::None; ///< 移除视口生产者时传给占位或旧图, 区分等待和硬拒绝.
        bool viewportPublished = false; ///< 仅完整正常图采样并实际发布时确认输入映射.
    };

    // 在离开图/准备作用域前复制阶段和原始 RHI 首错, 不让诊断借用图字符串或 arena.
    static AttemptResult Failed(Platform::DeviceHost &host, AttemptStatus status, std::string_view stage,
                                const Core::BoundedText &reason) noexcept
    {
        Core::BoundedText message(stage);
        message.Append(": ");
        message.Append(reason.View());
        if (auto native = host.FirstGraphicsError(); native && native->View() != reason.View())
        {
            message.Append(" | RHI: ");
            message.Append(native->View());
        }
        return {host.HasFatalGraphicsError() ? AttemptStatus::Stopped : status, std::move(message)};
    }

    /// @brief 本图颜色版本和实际上传账单. 不保存到跨帧 owner.
    struct FrameDeclaration
    {
        TextureId color;
        TextureId viewport;
        std::optional<ForwardGraph> scene;
        PassId diagnosticUpload;
        std::vector<GraphUpload> uploads = std::vector<GraphUpload>(0);
    };

    /// @brief 同槽附加请求自己的候选及准备事实. RenderFrame 保有到一次尝试结束.
    struct OffscreenCandidate
    {
        OffscreenViewRequest *request{};
        PreparedView prepared;
        PreparedViewOutput outputs;
        std::optional<Core::BoundedText> error;
        bool planned = false;
    };

    /// @brief 仅主输出被实际采样时进入其 Forward 分支.
    static bool UsesMainScene(const PreparedViewOutput &output, const Editor::PreparedUiFrame *ui,
                              Editor::UiFrameMode mode) noexcept
    {
        return mode == Editor::UiFrameMode::Normal && output.Color() && output.Color()->scene &&
            ui && ui->UsesViewOutput();
    }

    // 主视口按实际 UI 采样选择新生产者或导入旧颜色. 附加 View 通过独立的 ShaderRead 出口保持活性.
    static GraphResult<FrameDeclaration> DeclareFrame(RenderGraph &graph, const Platform::PresentTarget &target,
        PreparedViewOutput &outputs, Editor::PreparedUiFrame *prepared, Editor::UiFrameMode mode,
        FrameSlot &slot, GraphExecutionDiagnostics diagnostics, bool offscreen)
    {
        FrameDeclaration declared;
        auto imported = graph.ImportTexture(target.texture, {target.entryState, target.finalState, false});
        if (const auto *error = std::get_if<GraphError>(&imported))
        {
            return *error;
        }
        auto cleared = graph.AddPass<TextureId>("Window clear", [&](PassBuilder &builder, auto &data)
        {
            declared.color = data = builder.Write(std::get<TextureId>(imported), GraphUsage::ColorAttachment,
                {.load = LoadOp::Clear, .clearValue = RHI::Color(0.018f, 0.024f, 0.035f, 1.f)});
            return declared.color.IsValid();
        }, +[](PassResources &, const TextureId &, RHI::CommandList &) noexcept -> GraphFailure
        {
            return std::nullopt;
        });
        if (const auto *error = std::get_if<GraphError>(&cleared))
        {
            return *error;
        }
        const bool main = UsesMainScene(outputs, prepared, mode);
        if ((main || offscreen) && prepared)
        {
            // 共享写入准入要求 UI 和诊断纹理也先于场景缓冲.
            if (auto error = prepared->DeclareTextureUploads(graph, declared.color, slot.bindings, declared.uploads))
            {
                return *error;
            }
        }
        if (!main && outputs.Color() && prepared && prepared->UsesViewOutput())
        {
            if (!outputs.PrepareUpload(slot.arena))
            {
                return GraphError{GraphErrorCode::OutOfStorage, Core::BoundedText("Cannot prepare diagnostic pixels")};
            }
            auto viewport = outputs.Declare(graph, declared.uploads);
            if (const auto *error = std::get_if<GraphError>(&viewport))
            {
                return *error;
            }
            declared.viewport = std::get<TextureId>(viewport);
            if (outputs.UploadBytes() != 0)
            {
                declared.diagnosticUpload = declared.uploads.back().pass;
            }
        }
        if (main || offscreen)
        {
            auto forward = slot.forward.Declare(graph, declared.uploads, {main, offscreen});
            if (const auto *error = std::get_if<GraphError>(&forward))
            {
                return *error;
            }
            declared.scene = std::get<ForwardGraph>(std::move(forward));
            if (main)
            {
                declared.viewport = declared.scene->views[MainViewIndex]->displayColor;
            }
            if (offscreen)
            {
                const auto color = declared.scene->views[OffscreenViewIndex]->displayColor;
                if (auto error = graph.Export(color, ExportUsage::ShaderRead))
                {
                    return *error;
                }
            }
        }
        if (prepared)
        {
            auto composed = prepared->BuildGraph(graph, declared.color, declared.viewport,
                                                 slot.bindings, declared.uploads, diagnostics);
            if (const auto *error = std::get_if<GraphError>(&composed))
            {
                return *error;
            }
            declared.color = std::get<TextureId>(composed);
        }
        if (auto error = graph.Export(declared.color, ExportUsage::Presentation))
        {
            return *error;
        }
        if (declared.scene)
        {
            // 主路已接入窗口的 UI 采样, 附加路已导出显示色. 只为这些消费者保留新生产的完整数组.
            for (std::size_t index = 0; index < ForwardViewCount; ++index)
            {
                const auto &output = declared.scene->views[index];
                if (!output || !output->shadowDepth.IsValid())
                {
                    continue;
                }
                const auto &depth = *slot.forward.PreparedShadowDepth(index);
                if (depth.retained)
                {
                    continue;
                }
                if (auto error = graph.Export(output->shadowDepth, ExportUsage::ShaderRead,
                    TextureRange{.layerCount = depth.count, .aspect = TextureAspect::Depth}))
                {
                    return *error;
                }
            }
        }
        return declared;
    }

    // 成功 Close 恢复 keepInitialState. 失败目标将换代, 其它持久导入必须仍满足下次入口事实.
    // 不推测未执行到哪个 Pass, 按整个存活计划保守检查可能已消费的导入.
    // 池导入还检查所保留出口是否可读. 该检查会影响本轮能否继续处理后续帧, UI 请求由其 owner 确认.
    static bool ImportsCanRetry(const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Texture *target) noexcept
    {
        for (const auto &texture : plan.textures)
        {
            const auto &source = graph.Textures()[texture.resource.Identity().index];
            if (texture.importState && source.imported.Get() != target && (source.retained ?
                !source.retained.Readable() :
                (!source.desc.keepInitialState || source.desc.initialState != texture.importState->entry)))
            {
                return false;
            }
        }
        for (const auto &buffer : plan.buffers)
        {
            const auto &source = graph.Buffers()[buffer.resource.Identity().index];
            if (buffer.importState && (source.retained ? !source.retained.Readable() :
                (!source.desc.keepInitialState || source.desc.initialState != buffer.importState->entry)))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief 继承本轮 CPU 准备事实, 图候选只补充自己实际执行的阶段.
    struct FrameMeasurements
    {
        FrameStatistics &statistics;
        Editor::FrameCpuSeconds cpu;
        ForwardResidencyCounts residency;
        Editor::ScenePreparationStatistics preparation;
    };

    /// @brief 只为完成裁剪的 View 记录候选和选中规模. 输出资格由接受回执单独记录.
    static Editor::ViewCullingStatistics MeasureCulling(const PreparedView &view) noexcept
    {
        if (!view.camera)
        {
            return {};
        }
        return {view.view, view.cullingSeconds, view.scene.primitives.size(), view.primitives.size(),
                view.scene.lights.size(), view.lights.size()};
    }

    /// @brief 按来源区分实际执行的场景行, View 和批次写入, 并读取两份 arena 的累计峰值.
    static void CompletePreparationStatistics(Editor::ScenePreparationStatistics &statistics, const FrameSlot &slot,
        const FrameDeclaration &declared, const CompiledGraphPlan &plan, bool main, bool offscreen) noexcept
    {
        statistics.views[MainViewIndex].accepted = main;
        statistics.views[OffscreenViewIndex].accepted = offscreen;
        for (const auto &upload : declared.uploads)
        {
            if (upload.source == GraphUploadSource::Other || !IsLivePass(plan, upload.pass))
            {
                continue;
            }
            auto *counts = &statistics.viewUploads;
            if (upload.source == GraphUploadSource::ForwardStatic)
            {
                counts = &statistics.staticUploads;
            }
            else if (upload.source == GraphUploadSource::ForwardDraw)
            {
                counts = &statistics.drawUploads;
            }
            ++counts->writes;
            counts->bytes += upload.bytes;
        }
        statistics.payload = {slot.arena.Capacity(), slot.arena.HighWater(), slot.arena.GrowthCount()};
        statistics.graph = {slot.graphArena.Capacity(), slot.graphArena.HighWater(), slot.graphArena.GrowthCount()};
    }

    // Open 后所有失败留在原录制, 依次完成图执行, Close, 提交, 发布资格核对和取得消费.
    // Close/交接区间独立检查, 但不能清除原图的失败结论. 无可靠票据时保持原 owner 等待退出收拢.
    static AttemptResult ExecuteFrame(Platform::DeviceHost &host, Editor::EditorSession &editor,
        View &view, const Platform::PresentationFrame &presentation, FrameSlot &slot, RHI::CommandList &commands,
        Core::Diagnostics &diagnostics, RenderGraph &graph, const CompiledGraphPlan &plan,
        const FrameDeclaration &declared, PreparedViewOutput &outputs, Editor::PreparedUiFrame *prepared,
        Editor::UiFrameMode mode, Platform::RhiErrorScope &errors, GraphExecutionDiagnostics graphErrors,
        bool &opened, FrameMeasurements measurements, OffscreenCandidate *offscreen, GraphPoolReservation &reservation)
    {
        std::optional<Core::BoundedText> failure;
        const auto stopped = [&](std::string_view stage, const Core::BoundedText &reason)
        {
            auto result = Failed(host, AttemptStatus::Stopped, stage, reason);
            if (failure)
            {
                result.additional = std::move(result.error);
                result.error = failure;
            }
            return result;
        };
        GraphExecutionOutputs execution;
        // 从 Open 开始不再允许重建候选. 即使首次物化失败, 也须按录制或退出合同结束这一尝试.
        slot.diagnosticUploadBytes = IsLivePass(plan, declared.diagnosticUpload) ? outputs.UploadBytes() : 0;
        opened = true;
        commands.Open();
        if (!errors.IsClear())
        {
            return Failed(host, AttemptStatus::Stopped, "Frame.Open", Core::BoundedText("Cannot open Graphics list"));
        }
        commands.CommitBarriers();
        if (host.HasFatalGraphicsError())
        {
            return Failed(host, AttemptStatus::Stopped, "Frame.Open",
                          Core::BoundedText("Fatal before graph execution"));
        }
        // 以本图实际声明的 Forward 分支界定候选. 记录区间含场景和 UI, 到图执行结束即截止.
        const bool measure = declared.scene.has_value();
        const auto before = measure ? host.ReadGraphicsCounts() : RHI::GraphicsOperationCounts{};
        auto timer = measure && errors.IsClear() ? measurements.statistics.ReserveTimer() :
            FrameStatistics::TimerReservation{};
        std::optional<Core::BoundedText> timerBeginError;
        if (timer.query)
        {
            commands.BeginTimerQuery(timer.query);
            if (!errors.IsClear())
            {
                bool reusable = false;
                if (!host.HasFatalGraphicsError())
                {
                    timerBeginError = host.FirstGraphicsError();
                    // 起点拒绝可发生在时间戳发出前. 由 RHI 确认原查询可重置, 成功则保留池项.
                    const auto reset = host.CaptureRhiErrors();
                    host.GetDevice()->ResetTimerQuery(timer.query);
                    reusable = reset.IsClear();
                }
                if (!reusable)
                {
                    measurements.statistics.FailTimer(timer);
                }
                timer = {.state = Editor::GpuIntervalState::Failed};
                if (!host.HasFatalGraphicsError())
                {
                    // 计时前区间已干净. 为实际图和其消费者重开同一个诊断对象, 由图接管确认包仍有效.
                    // 原生起点失败已使包或设备失效, 接管仍会拒绝.
                    errors = host.BeginFrameGraphicsDiagnostics();
                }
            }
        }
        const auto graphStorageBefore = host.ReadGraphicsCounts();
        const auto graphStart = CpuClock::now();
        auto executed = ExecuteRenderGraph(graph, plan, *host.GetDevice(), commands, graphErrors, &reservation);
        measurements.cpu.graphExecute = SecondsSince(graphStart);
        const auto afterGraph = host.ReadGraphicsCounts();
        const auto native = measure ? afterGraph - before : RHI::GraphicsOperationCounts{};
        measurements.statistics.GraphStorageCreated(afterGraph - graphStorageBefore);
        if (timer.query)
        {
            // 保留正常录制的配对义务. 失败候选即使有完整区间也只获得退休资格.
            const auto end = host.CaptureRhiErrors();
            commands.EndTimerQuery(timer.query);
            if (!end.IsClear())
            {
                measurements.statistics.FailTimer(timer);
                timer = {.state = Editor::GpuIntervalState::Failed};
            }
        }
        if (const auto *error = std::get_if<GraphError>(&executed))
        {
            // 接管拒绝时保留可能造成包失效的原始时间戳错误. 图内其余失败使用自己的首错.
            failure = timerBeginError && error->code == GraphErrorCode::InvalidRecording ?
                Failed(host, AttemptStatus::Cancelled, "Frame.TimerBegin", *timerBeginError).error :
                Failed(host, AttemptStatus::Cancelled, "Frame.Execute", error->description).error;
        }
        else
        {
            execution = std::move(std::get<GraphExecutionOutputs>(executed));
            if (FindTextureOutput(*execution, declared.color) != presentation.target.texture.Get())
            {
                failure.emplace("Graph did not return the acquired window color");
            }
        }
        if (!errors.IsClear() && !failure)
        {
            failure.emplace("Graphics execution reported an error");
        }
        if (host.HasFatalGraphicsError())
        {
            return {AttemptStatus::Stopped, failure, {}};
        }
        // 图错误已复制到 failure. 单独取样 Close, 允许合法前缀退休而不把原图重新判成成功.
        const auto closeErrors = host.CaptureRhiErrors();
        commands.Close();
        if (!closeErrors.IsClear())
        {
            return stopped("Frame.Close", Core::BoundedText("Cannot close Graphics list"));
        }
        const bool recorded = !failure;
        const auto handoffErrors = host.CaptureRhiErrors();
        if (auto error = host.PrepareSubmission(presentation.token, commands, recorded))
        {
            return stopped("Frame.Presentation", Platform::DescribeHostError(*error));
        }
        if (!handoffErrors.IsClear())
        {
            return stopped("Frame.Presentation", Core::BoundedText("Window color handoff failed"));
        }
        std::optional<Platform::HostError> hostError;
        const auto receipt = host.SubmitPresentation(presentation.token, hostError);
        // 非零 ticket 先记账. 随后的 Event 或诊断失败只撤销继续运行资格, 不能抹去已接受工作.
        slot.lastSubmission = receipt.ticket;
        const bool accepted = recorded && receipt.colorValid && receipt.ticket != 0 && !hostError &&
            handoffErrors.IsClear();
        measurements.statistics.GraphReceipt(receipt.ticket, accepted);
        if (receipt.ticket != 0)
        {
            ReportTicket(diagnostics, "Window Graphics submission accepted", receipt.ticket);
        }
        if (hostError || !handoffErrors.IsClear() || receipt.ticket == 0)
        {
            return stopped("Frame.Submit", hostError ? Platform::DescribeHostError(*hostError) :
                                                       Core::BoundedText("Submission has no reliable receipt"));
        }
        const bool usedViewport = prepared && prepared->UsesViewOutput();
        // 确认只消费当前图实际执行的 UI 请求. 安全图可以确认自己的上传, 但永不发布旧 View 快照.
        if (prepared)
        {
            const bool confirmed = prepared->Finish({commands, plan, execution.get(), receipt.ticket, accepted});
            assert(confirmed == accepted);
            static_cast<void>(confirmed);
        }
        const auto publishView = [&](View &owner, PreparedViewOutput &candidate, TextureId color)
        {
            // 场景出口须能追溯到本槽的程序表. View 还要用导出纹理和票据确认自己的请求版本.
            const auto *output = FindTextureExport(*execution, color);
            const auto *planned = candidate.Color();
            if (planned && planned->scene && planned->scene->shaderGeneration != slot.forward.ShaderGeneration())
            {
                return false;
            }
            return output && owner.Publish(std::move(candidate), output->object, output->retained, receipt.ticket);
        };
        const bool published = accepted && usedViewport && mode == Editor::UiFrameMode::Normal &&
            publishView(view, outputs, declared.viewport);
        if (accepted && declared.scene)
        {
            // 共享存储按本计划的写入确认. 每路显示色另外用自己的出口查找原生对象, 再核对所属 View 的请求.
            slot.forward.Confirm(*declared.scene, plan);
            const auto confirmDepth = [&](View &owner, std::size_t index)
            {
                // 调用处已确认本路颜色发布成功. 只有新生产的组有深度出口, 只读命中继续沿用原回执.
                const auto depth = declared.scene->views[index]->shadowDepth;
                if (depth.IsValid())
                {
                    if (const auto *output = FindTextureExport(*execution, depth))
                    {
                        slot.forward.ConfirmShadowDepth(owner, index, output->retained, receipt.ticket);
                    }
                }
            };
            if (published && declared.scene->views[MainViewIndex])
            {
                confirmDepth(view, MainViewIndex);
                measurements.statistics.PublishAcceptedShadows(MainViewIndex, view);
            }
            if (offscreen)
            {
                auto &request = *offscreen->request;
                const auto color = declared.scene->views[OffscreenViewIndex]->displayColor;
                if (publishView(request.view, offscreen->outputs, color))
                {
                    confirmDepth(request.view, OffscreenViewIndex);
                    measurements.statistics.PublishAcceptedShadows(OffscreenViewIndex, request.view);
                    request.reason = ViewSettingReason::None;
                    request.submission = receipt.ticket;
                }
            }
        }
        const auto result = host.Present(presentation.token, hostError);
        if (hostError)
        {
            return stopped("Frame.Present", Platform::DescribeHostError(*hostError));
        }
        std::optional<Editor::SceneFrameIdentity> measuredSource;
        const bool measuredMain = published && measure && declared.scene->views[MainViewIndex].has_value();
        const bool measuredOffscreen = offscreen && offscreen->request->submission != 0;
        if (measuredMain || measuredOffscreen)
        {
            // 单个统计来源优先保留主 View, 纯离屏正常帧使用实际接受的附加 View.
            const auto &measured = measuredMain ? view : offscreen->request->view;
            const auto retained = measured.LastAcceptedOutput();
            const auto &last = *retained.Color()->scene;
            measuredSource = Editor::SceneFrameIdentity{receipt.ticket, last.scene, measured.Id(), last.contentRevision,
                last.documentRevision, last.mode, last.renderLayout, retained.Color()->layout, last.display,
                last.shaderGeneration, last.directionalCascades, last.localShadow};
            const auto index = measuredMain ? MainViewIndex : OffscreenViewIndex;
            // 上传账单汇总整张正常图. 几何工作量只取所选出口, 用其深度和颜色 Pass 的存活性分别计数.
            CompletePreparationStatistics(measurements.preparation, slot, declared, plan, measuredMain,
                                          measuredOffscreen);
            measurements.statistics.Publish({*measuredSource, measurements.cpu, native, host.ReadUploadStorage(),
                slot.forward.ExecutedWorkCounts(index, *declared.scene->views[index], plan),
                measurements.residency, plan.passes.size(), result.swapped,
                result.attempted ? std::optional(result.presentWaitSeconds) : std::nullopt, timer.state,
                measurements.preparation});
        }
        if (timer.query)
        {
            measurements.statistics.FinishTimer(timer, measuredSource ? &*measuredSource : nullptr);
        }
        if (failure)
        {
            // Present 已消耗无效颜色的取得, 未交换. 能否继续还取决于基础 UI 和其它持久导入.
            if (!editor.BaseFontReady())
            {
                return stopped("Frame.InitialUi", Core::BoundedText("Required base font was not accepted"));
            }
            if (!ImportsCanRetry(graph, plan, presentation.target.texture.Get()))
            {
                return stopped("Frame.Retry", Core::BoundedText("Persistent import states cannot be re-established"));
            }
            return {AttemptStatus::Cancelled, failure, {}};
        }
        if (result.swapped)
        {
            ReportTicket(diagnostics, "Editor window presented", result.ticket);
        }
        return {.status = AttemptStatus::Succeeded, .viewportPublished = published};
    }

    // 先准备 UI 并编译图. 按存活上传账单检查设备容量, 再向会话图池预留本计划的物理槽, 两者就绪才 Open.
    // 失败返回按局部对象逆序结束计划, 图和准备值, 调用者随后才可 reset 槽.
    // 宿主首错捕获持续到下一候选或 Shutdown; 本次错误须在返回前复制到 AttemptResult.
    static AttemptResult TryFrame(Platform::DeviceHost &host, Editor::EditorSession &editor, View &view,
        const Platform::PresentationFrame &presentation, FrameSlot &slot, RHI::CommandList &commands,
        Core::Diagnostics &diagnostics, PreparedViewOutput &outputs, Editor::UiFrameMode mode,
        FrameMeasurements measurements, OffscreenCandidate *offscreen, GraphResourcePool &graphPool)
    {
        bool opened = false;
        try
        {
            auto errors = host.BeginFrameGraphicsDiagnostics();
            const GraphExecutionDiagnostics graphErrors{&errors, +[](const void *scope) noexcept
            {
                return static_cast<const Platform::RhiErrorScope *>(scope)->IsClear();
            }};
            const auto preparationFailure = editor.BaseFontReady() ? AttemptStatus::Cancelled : AttemptStatus::Stopped;
            const auto uiStart = CpuClock::now();
            auto ui = editor.PrepareUiFrame(presentation.target.extentPixels, slot.arena, outputs, mode);
            measurements.cpu.ui += SecondsSince(uiStart);
            if (const auto *error = std::get_if<Core::BoundedText>(&ui))
            {
                return Failed(host, preparationFailure, "Frame.UiPrepare", *error);
            }
            if (!errors.IsClear())
            {
                return Failed(host, preparationFailure, "Frame.UiPrepare", Core::BoundedText("UI preparation failed"));
            }
            auto *prepared = std::get_if<Editor::PreparedUiFrame>(&ui);
            const auto declareStart = CpuClock::now();
            const auto sceneBytes = UsesMainScene(outputs, prepared, mode) || offscreen ?
                slot.forward.GraphStorageBytes() : 0;
            if (!slot.graphArena.Reserve((prepared ? prepared->GraphStorageBytes() : 0) + 1024 + sceneBytes))
            {
                return Failed(host, preparationFailure, "Frame.Prepare",
                              Core::BoundedText("Cannot reserve graph storage"));
            }
            RenderGraph graph(slot.graphArena, host.GetDevice());
            auto declaration = DeclareFrame(graph, presentation.target, outputs, prepared, mode, slot, graphErrors,
                                            offscreen != nullptr);
            // 其它图消费者的准备也属于此边界, Fatal 不能被编译或安全图覆盖.
            if (const auto *error = std::get_if<GraphError>(&declaration))
            {
                return Failed(host, AttemptStatus::GraphRejected, "Frame.Declare", error->description);
            }
            if (!errors.IsClear())
            {
                return Failed(host, AttemptStatus::GraphRejected, "Frame.Declare",
                              Core::BoundedText("Consumer preparation failed"));
            }
            if (auto error = graph.Seal())
            {
                return Failed(host, AttemptStatus::GraphRejected, "Frame.Seal", error->description);
            }
            measurements.cpu.graphDeclare = SecondsSince(declareStart);
            const auto &declared = std::get<FrameDeclaration>(declaration);
            const auto sceneCount = declared.scene ? std::ranges::count_if(declared.scene->views,
                [](const auto &source) { return source.has_value(); }) : 0;
            std::uint64_t shadowBytes = 0;
            if (declared.scene)
            {
                for (const auto &source : declared.scene->views)
                {
                    if (source)
                    {
                        shadowBytes += source->shadowBytes;
                    }
                }
            }
            EditorGraphProfile profile(host, static_cast<std::size_t>(sceneCount), declared.diagnosticUpload.IsValid(),
                                       shadowBytes);
            if (!errors.IsClear())
            {
                return Failed(host, preparationFailure, "Frame.Profile",
                              Core::BoundedText("Cannot prepare graph capabilities"));
            }
            const auto compileStart = CpuClock::now();
            auto compiled = CompileRenderGraph(graph, profile.Options());
            measurements.cpu.graphCompile = SecondsSince(compileStart);
            if (const auto *error = std::get_if<GraphError>(&compiled))
            {
                if (error->code == GraphErrorCode::TransientBudgetExceeded)
                {
                    measurements.statistics.ReportPoolPressure(error->description);
                }
                return Failed(host, AttemptStatus::GraphRejected, "Frame.Compile", error->description);
            }
            const auto &plan = *std::get<std::unique_ptr<const CompiledGraphPlan>>(compiled);
            // 此时可以发布编译规模. 中途取消仍保留这次计划, 原生创建数和提交资格在各自发生时补入.
            const auto imports = [](const auto &resources)
            {
                return static_cast<std::size_t>(std::ranges::count_if(resources,
                    [](const auto &resource) { return resource.importState.has_value(); }));
            };
            measurements.statistics.PublishGraph({.generation = plan.graphGeneration, .plannedBytes = plan.transientBytes,
                .declared = graph.Passes().size(), .live = plan.passes.size(),
                .imports = imports(plan.textures) + imports(plan.buffers), .exports = plan.exports.size(),
                // 主路使用旧色时, 显式离屏出口仍可保住场景生产者. 只有纯 UI 图标为安全图.
                .slots = plan.transientSlots.size(), .safeUi = mode == Editor::UiFrameMode::SafeUi && !declared.scene});
            auto measured = CollectGraphUploads(plan, declared.uploads);
            if (const auto *error = std::get_if<GraphError>(&measured))
            {
                return Failed(host, AttemptStatus::GraphRejected, "Frame.UploadPlan", error->description);
            }
            const auto &budget = std::get<GraphUploadBudget>(measured);
            const auto admission = host.CheckUploadCapacity(commands.GetDesc().uploadChunkSize,
                                                           budget.textures, budget.buffers);
            if (!errors.IsClear())
            {
                return Failed(host, preparationFailure, "Frame.UploadAdmission",
                              Core::BoundedText("Upload admission failed"));
            }
            const bool liveSceneUpload = declared.scene && std::ranges::any_of(declared.uploads,
                [&](const auto &upload)
                {
                    return upload.source != GraphUploadSource::Other && IsLivePass(plan, upload.pass);
                });
            if ((admission == Platform::UploadAdmission::Pending || admission == Platform::UploadAdmission::ExceedsLimit) &&
                (liveSceneUpload || IsLivePass(plan, declared.diagnosticUpload)))
            {
                // 只在尚未开放列表时移除视口生产者, 重新为实际存活的 UI 写入准入.
                if (admission == Platform::UploadAdmission::Pending)
                {
                    return {AttemptStatus::RebuildWithoutViewport, {}, {}, ViewSettingReason::UploadPending};
                }
                return {AttemptStatus::RebuildWithoutViewport,
                        Core::BoundedText("Viewport and UI uploads exceed the Graphics list limit"), {},
                        declared.scene ? ViewSettingReason::SceneUploadLimit : ViewSettingReason::DiagnosticBudget};
            }
            if (admission == Platform::UploadAdmission::Pending)
            {
                return {AttemptStatus::UploadPending, {}};
            }
            if (admission != Platform::UploadAdmission::Ready)
            {
                return Failed(host, preparationFailure, "Frame.UploadAdmission",
                              Core::BoundedText("UI upload capacity rejected"));
            }
            auto reservation = graphPool.Admit(plan);
            if (const auto *error = std::get_if<GraphError>(&reservation))
            {
                measurements.statistics.ReportPoolPressure(error->description);
                auto rejected = Failed(host, AttemptStatus::GraphRejected, "Frame.GraphPool", error->description);
                rejected.viewportReason = ViewSettingReason::SceneOutputUnavailable;
                return rejected;
            }
            return ExecuteFrame(host, editor, view, presentation, slot, commands, diagnostics, graph, plan, declared,
                                outputs, prepared, mode, errors, graphErrors, opened, measurements, offscreen,
                                *std::get<std::unique_ptr<GraphPoolReservation>>(reservation));
        }
        catch (const std::bad_alloc &)
        {
            // 异常展开已结束 CPU 借用. Open 后可能有即时前缀, 必须交受控退出而不能退还为无工作取得.
            return Failed(host, opened ? AttemptStatus::Stopped : AttemptStatus::GraphRejected,
                          "Frame.Storage", Core::BoundedText("Candidate storage allocation failed"));
        }
        catch (const std::length_error &)
        {
            return Failed(host, opened ? AttemptStatus::Stopped : AttemptStatus::GraphRejected,
                          "Frame.Storage", Core::BoundedText("Candidate storage size is not representable"));
        }
    }

    /// @brief 有源几何的场景始终可输出已清除附件. 仅天空的物理 View 也有实际生产者.
    static bool HasSceneOutput(const PreparedView &prepared) noexcept
    {
        const bool sky = prepared.preview == PreviewMode::PhysicalLighting && prepared.scene.environment &&
            prepared.scene.environment->settings.sky;
        return prepared.reason == ViewSettingReason::None && prepared.camera && (prepared.scene.hasGeometry || sky);
    }

    void AdvanceFrameShaders(Platform::DeviceHost &host, ForwardRenderer &forward, FrameStatistics &statistics,
        Shader::SourceReload *reload, std::optional<Shader::ReloadResult> startupFailure, bool hasFramePlan) noexcept
    {
        auto result = reload ? reload->Poll() : std::move(startupFailure);
        const auto errors = host.BeginFrameGraphicsDiagnostics();
        forward.AdvanceShaderPreparation(reload ? reload->Requested() : 0, std::move(result));
        if (!errors.IsClear())
        {
            if (const auto error = host.FirstGraphicsError())
            {
                forward.RecordShaderDiagnostic(*error);
            }
        }
        if (hasFramePlan && !host.HasFatalGraphicsError() && forward.ShaderCandidateReady())
        {
            // 原生 link 期间仍可保存. 在提交前以零超时读已到达的通知, 之后只切换拥有关系.
            result = reload ? reload->Poll() : std::nullopt;
            forward.PublishShaderPreparation(reload ? reload->Requested() : 0, std::move(result));
        }
        statistics.PublishShaders(forward.ShaderStatistics(), reload ? reload->Statistics() : Shader::ReloadStatistics{});
    }

    /// @brief 将本路准备阶段的原生错误转为明确的 View 结果, 保留已有的具体失败.
    static std::optional<ForwardFailure> PrepareForwardView(Platform::DeviceHost &host, const auto &prepare)
    {
        const auto errors = host.BeginFrameGraphicsDiagnostics();
        auto failure = prepare();
        if (!errors.IsClear() && (!failure || failure->reason == ViewSettingReason::EnvironmentPending ||
                                  failure->reason == ViewSettingReason::ShaderPending))
        {
            return ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                                  Core::BoundedText("Forward resource preparation failed")};
        }
        return failure;
    }

    /// @brief 附加 View 在主 View 后独立校验和规划, 局部失败只对对应请求报告结果.
    static OffscreenCandidate PlanOffscreen(Platform::DeviceHost &host, ForwardRenderer &forward,
        FrameSlot &slot, SceneFramePreparation &scene, const View &main,
        std::span<OffscreenViewRequest> requests, const ShadowPlanningLimits &shadowLimits)
    {
        OffscreenCandidate candidate;
        if (requests.size() != 1)
        {
            for (auto &request : requests)
            {
                request.reason = ViewSettingReason::ViewCapacityLimit;
            }
            return candidate;
        }
        auto &request = requests.front();
        auto &view = request.view;
        if (view.Id() == main.Id() || (request.layout && request.layout->view != view.Id()))
        {
            // 外来布局或重复主 View 没有结算该 View 缓存的资格.
            request.reason = ViewSettingReason::InvalidViewRequest;
            return candidate;
        }
        // 已确认这一路的 View 身份. 即使其布局过期, Runtime 也可发布归属于它的拒绝摘要.
        candidate.request = &request;
        std::optional<ForwardFailure> ordinaryAttachmentFailure;
        if (request.layout)
        {
            const auto &layout = *request.layout;
            auto layoutReason = ViewSettingReason::InvalidViewRequest;
            if (layout.requestRevision == view.Settings().requestRevision)
            {
                // 同一相机在同尺寸画布上的有效矩形相同. 用尺寸先核对必需附件, 再核对普通预览的 FXAA.
                const bool resample = layout.renderExtent != layout.outputExtent;
                const auto display = ResolveDisplaySettings(view.Preview(), view.DocumentRequests().antiAliasing);
                const bool diagnosticCanAvoidFxaa = !std::holds_alternative<std::monostate>(
                    view.RequestedShadowDiagnostic()) &&
                    display.antiAliasing == AntiAliasing::Fxaa;
                const auto requiredDisplay = diagnosticCanAvoidFxaa ? DisplaySettings{} : display;
                const auto failure = forward.CheckAttachments(layout.renderExtent, layout.outputExtent,
                                                              requiredDisplay, resample);
                layoutReason = failure ? failure->reason : ViewSettingReason::None;
                if (failure)
                {
                    candidate.error = failure->description;
                }
                else if (diagnosticCanAvoidFxaa)
                {
                    // 线性路径已通过预检. 普通路径若超限, 先确认所选阴影组可供诊断消费.
                    ordinaryAttachmentFailure = forward.CheckAttachments(layout.renderExtent, layout.outputExtent,
                                                                         display, resample);
                }
            }
            if (layoutReason != ViewSettingReason::None)
            {
                request.reason = layoutReason;
                candidate.outputs = view.Unavailable(request.reason);
                candidate.prepared.view = view.Id();
                candidate.prepared.requestRevision = layout.requestRevision;
                candidate.prepared.documentRevision = view.DocumentRevision();
                candidate.prepared.reason = layoutReason;
                candidate.prepared.shadowStatistics = slot.offscreenView.UnavailableShadows(scene.Source(), view,
                                                                                              shadowLimits);
                return candidate;
            }
        }
        candidate.prepared = slot.offscreenView.Prepare(scene, view, request.layout, {}, ViewportUse::DisplayOutput,
                                                        shadowLimits, ordinaryAttachmentFailure.has_value());
        if (!HasSceneOutput(candidate.prepared))
        {
            request.reason = candidate.prepared.reason != ViewSettingReason::None ?
                candidate.prepared.reason : ViewSettingReason::SceneOutputUnavailable;
            candidate.outputs = view.Unavailable(request.reason);
            candidate.error = ordinaryAttachmentFailure && request.reason == ordinaryAttachmentFailure->reason ?
                std::optional{ordinaryAttachmentFailure->description} : candidate.prepared.failure;
            return candidate;
        }

        const auto &prepared = candidate.prepared;
        const auto failure = PrepareForwardView(host, [&] {
            return forward.PlanView(slot.forward, prepared, prepared.preview, {}, OffscreenViewIndex);
        });
        if (failure)
        {
            request.reason = failure->reason;
            candidate.outputs = view.Unavailable(request.reason);
            if (request.reason != ViewSettingReason::EnvironmentPending)
            {
                candidate.error = Failed(host, AttemptStatus::Cancelled, "Frame.OffscreenPlan", failure->description).error;
            }
        }
        candidate.planned = !failure;
        return candidate;
    }

    /// @brief 仅已完成规划的附加路从本帧选定代取得句柄并形成正常候选.
    static void FinishOffscreen(Platform::DeviceHost &host, ForwardRenderer &forward, FrameSlot &slot,
                                OffscreenCandidate &candidate)
    {
        if (!candidate.planned)
        {
            return;
        }
        auto &request = *candidate.request;
        auto &view = request.view;
        const auto &prepared = candidate.prepared;
        const auto failure = PrepareForwardView(host, [&] {
            return forward.FinishView(slot.forward, prepared, OffscreenViewIndex);
        });
        if (failure)
        {
            request.reason = failure->reason;
            candidate.outputs = view.Unavailable(request.reason);
            if (request.reason != ViewSettingReason::EnvironmentPending &&
                request.reason != ViewSettingReason::ShaderPending)
            {
                candidate.error = Failed(host, AttemptStatus::Cancelled, "Frame.OffscreenPrepare",
                                         failure->description).error;
            }
            return;
        }
        auto output = view.PreparePreview(prepared, slot.forward.Degradation(OffscreenViewIndex),
            slot.forward.Environment(OffscreenViewIndex), slot.forward.ShaderGeneration(),
            slot.forward.PreparedShadowDepth(OffscreenViewIndex));
        if (const auto *error = std::get_if<Core::BoundedText>(&output))
        {
            request.reason = ViewSettingReason::ScenePreparationFailed;
            candidate.outputs = view.Unavailable(request.reason);
            candidate.error = *error;
            return;
        }
        candidate.outputs = std::get<PreparedViewOutput>(std::move(output));
        request.reason = ViewSettingReason::SubmissionPending;
    }

    // 一次取得最多尝试带视口的普通图, 去掉视口的普通图和安全 UI 图.
    // 移除场景分支后, 无旧图可用时由普通 UI 画占位, 可保留的旧图只在 SafeUi 合成; 两者都不再上传本轮场景.
    // 可恢复分支在此消费取得或沿已提交前缀返回; Stopped 将尚未收拢的 owner 留给主循环退出.
    std::variant<FrameStatus, Core::BoundedText> RenderFrame(
        Platform::DeviceHost &host, Editor::EditorSession &editor, View &view, ForwardRenderer &forward,
        const Editor::EditorFrame &editorFrame, const Platform::PresentationFrame &presentation, FrameSlot &slot,
        std::size_t remainingDiagnosticBytes, RHI::CommandList &commands, Core::Diagnostics &diagnostics,
        FrameStatistics &statistics, GraphResourcePool &graphPool, double editorSeconds,
        std::span<OffscreenViewRequest> offscreenRequests, Shader::SourceReload *reload,
        std::optional<Shader::ReloadResult> startupShaderFailure)
    {
        statistics.BeginGraphFrame();
        // 即使候选在准备前终止, 复用的请求也不能带回上一帧的接受票据.
        for (auto &request : offscreenRequests)
        {
            request.submission = 0;
            request.reason = ViewSettingReason::ScenePreparationFailed;
        }
        std::optional<Core::BoundedText> primary;
        std::optional<Core::BoundedText> additional;
        // 普通失败记录一次, 后续失败只作为附加原因. 会话拥有副本, 不依赖本函数或图存活.
        const auto remember = [&](const std::optional<Core::BoundedText> &error)
        {
            if (error)
            {
                (primary ? additional : primary) = *error;
                static_cast<void>(diagnostics.Write(Core::Severity::Error, error->View()));
            }
        };
        const auto stop = [&]() -> std::variant<FrameStatus, Core::BoundedText>
        {
            editor.SetAcceptedViewport(nullptr, view);
            editor.ReportFrameOutcome(Editor::FrameRecovery::Stopped, primary, additional);
            return primary.value_or(Core::BoundedText("Graphics work requires controlled shutdown"));
        };
        if (host.HasFatalGraphicsError())
        {
            remember(host.FirstGraphicsError());
            return stop();
        }
        // 本轮事务已提交且同槽已 Ready. 在图尝试外准备, 安全 UI 和上传重试不再读取 Scene.
        const auto sceneStart = CpuClock::now();
        slot.forward.Reset();
        forward.BeginShaderRequests();
        const auto *scene = editor.ActiveScene();
        assert(scene);
        const auto sceneCameraRevision = scene->Versions().camera;
        const auto &environment = scene->Environment();
        forward.BeginEnvironmentRequests(environment ? environment->source : nullptr);
        // 同帧各路共用 D32 数组的设备上限, 预算与层地址则按各自的 View 计算.
        ShadowPlanningLimits shadowLimits;
        static_cast<void>(host.QueryDepthArrayLimits(ShadowDepthFormat, shadowLimits.maxEdge, shadowLimits.maxLayers));
        SceneFramePreparation shared(slot.sceneFrame, *scene);
        const auto prepared = slot.mainView.Prepare(shared, view, editorFrame.viewport, {}, ViewportUse::EditorViewport,
                                                    shadowLimits);
        editor.SetPreparedViewport(&prepared);
        // 局部 View 失败仍进入诊断占位和基础 UI, 这里记录原因而不撤销其它可用 UI.
        remember(prepared.failure);
        std::variant<PreparedViewOutput, Core::BoundedText> candidate;
        const bool physical = prepared.preview == PreviewMode::PhysicalLighting;
        const bool hasScene = HasSceneOutput(prepared);
        // 阴影、物理预览和缩放的候选失败时, 可读旧色仍能由安全 UI 显示.
        const bool retainOnFailure = physical || (hasScene &&
            (!std::holds_alternative<std::monostate>(prepared.shadowDiagnostic) ||
             (prepared.preview == PreviewMode::DirectLighting && prepared.shadowStatistics.enabled) ||
             prepared.layouts.render != prepared.layouts.display));
        auto mode = Editor::UiFrameMode::Normal;
        const auto viewportFallback = [&](ViewSettingReason reason, bool canRetain)
        {
            auto output = view.Unavailable(reason);
            if (canRetain)
            {
                auto retained = view.LastAcceptedOutput(reason);
                if (retained.Color())
                {
                    mode = Editor::UiFrameMode::SafeUi;
                    return retained;
                }
            }
            return output;
        };
        // 先封存两路的实际计划. 能消费场景的主路才登记驻留和上传, 离屏继续独立声明自己的需求.
        const bool mainRequested = hasScene && editor.HasViewportSample(presentation.target.extentPixels);
        std::optional<ForwardFailure> mainFailure;
        if (mainRequested)
        {
            mainFailure = PrepareForwardView(host, [&] {
                return forward.PlanView(slot.forward, prepared, prepared.preview);
            });
        }
        if (host.HasFatalGraphicsError())
        {
            statistics.PublishShadows(MeasureShadows(prepared, sceneCameraRevision));
            remember(host.FirstGraphicsError());
            return stop();
        }
        auto offscreen = PlanOffscreen(host, forward, slot, shared, view, offscreenRequests, shadowLimits);
        if (offscreen.request)
        {
            // 附加路可能换成另一 View. 先从其 owner 读取自己的接受历史, 即使本轮布局已被拒绝.
            statistics.PublishAcceptedShadows(OffscreenViewIndex, offscreen.request->view);
        }
        // 在图编译和可能的安全 UI 重试之前发布. 此后的接受票据只确认实际提交的场景颜色.
        statistics.PublishShadows(MeasureShadows(prepared, sceneCameraRevision),
            offscreen.request ? std::optional(MeasureShadows(offscreen.prepared, sceneCameraRevision)) : std::nullopt);
        forward.EndEnvironmentRequests();
        if (!host.HasFatalGraphicsError())
        {
            AdvanceFrameShaders(host, forward, statistics, reload, std::move(startupShaderFailure), true);
        }
        if (host.HasFatalGraphicsError())
        {
            remember(host.FirstGraphicsError());
            return stop();
        }

        // 到此本帧代已选定. 正常 View 从同一张表取得句柄, 未就绪的路可沿自己的旧接受输出回退.
        if (mainRequested)
        {
            if (!mainFailure)
            {
                mainFailure = PrepareForwardView(host, [&] {
                    return forward.FinishView(slot.forward, prepared);
                });
            }
            if (const auto &failure = mainFailure)
            {
                // 环境烘焙和 Shader 准备的正常等待只更新 View 原因, 实际拒绝仍写入本次帧诊断.
                if (failure->reason != ViewSettingReason::EnvironmentPending &&
                    failure->reason != ViewSettingReason::ShaderPending)
                {
                    remember(Failed(host, AttemptStatus::Cancelled, "Frame.ScenePrepare", failure->description).error);
                }
                candidate = viewportFallback(failure->reason, retainOnFailure ||
                    failure->reason == ViewSettingReason::ShaderPending ||
                    failure->reason == ViewSettingReason::ScenePipelineUnavailable);
            }
            else
            {
                candidate = view.PreparePreview(prepared, slot.forward.Degradation(), slot.forward.Environment(),
                                                slot.forward.ShaderGeneration(), slot.forward.PreparedShadowDepth());
            }
            if (host.HasFatalGraphicsError())
            {
                return stop();
            }
        }
        else if (hasScene)
        {
            // 封存命令不采样时没有本轮颜色回执. 保留上次显示代, 仅更新当前请求的等待原因.
            candidate = view.Unavailable(ViewSettingReason::ViewportSamplingPending);
        }
        else
        {
            candidate = view.PrepareDiagnostic(prepared, remainingDiagnosticBytes);
        }
        if (const auto *error = std::get_if<Core::BoundedText>(&candidate))
        {
            remember(*error);
            return stop();
        }
        auto outputs = std::move(std::get<PreparedViewOutput>(candidate));
        FinishOffscreen(host, forward, slot, offscreen);
        remember(offscreen.error);
        if (host.HasFatalGraphicsError())
        {
            return stop();
        }
        // 只有准备出本轮颜色候选的附加路才进入图. 响应对象即使被拒绝也保留原因供调用方读取.
        auto *selectedOffscreen = offscreen.outputs.Color() ? &offscreen : nullptr;
        const auto *extracted = shared.Result();
        // 两路 View 准备结束后交付成功提取的模型表. 诊断帧据此退休旧代, 常规 Forward 已观察的同一版本可直接复用.
        if (extracted)
        {
            if (const auto failure = forward.ObserveModelTable(*extracted))
            {
                remember(Failed(host, AttemptStatus::Cancelled, "Frame.SceneResidency", failure->description).error);
            }
        }
        // 在构图合并及裁剪前取准备需求. ExecuteFrame 只在场景出口获接受后随对应票据发布这些值.
        const auto [requestedWrites, requestedBytes] = slot.forward.UploadDemand();
        const FrameMeasurements measurements{statistics, {editorSeconds, SecondsSince(sceneStart)},
            forward.ResidencyCounts(),
            {.slot = presentation.token.slotIndex, .structureRevision = extracted ? extracted->structureRevision : 0,
             .extraction = extracted ? extracted->extraction : SceneExtractionStatistics{},
             .encodings = slot.forward.EncodingCounts(), .views = {MeasureCulling(prepared),
                                                                  MeasureCulling(offscreen.prepared)},
             .uploadDemand = {requestedWrites, requestedBytes}}};
        // 本次取得已证明同槽上次工作完成. 后续失败分支只检查本轮是否真的提交过前缀.
        slot.lastSubmission = 0;
        slot.diagnosticUploadBytes = 0;
        for (;;)
        {
            // 首轮已由同槽 Ready 证明安全; 后继候选只会在无录制且旧尝试完全析构后到达.
            // 图 arena 已由 RenderGraph 析构回退; 这里重置独立载荷区和仅属于旧图的 token 绑定.
            slot.arena.Reset();
            slot.bindings.Reset();
            const auto attempt = TryFrame(host, editor, view, presentation, slot, commands, diagnostics, outputs, mode,
                                          measurements, selectedOffscreen, graphPool);
            statistics.PublishPool(graphPool.Statistics());
            const bool attemptedOffscreen = selectedOffscreen != nullptr;
            selectedOffscreen = nullptr;
            if (attemptedOffscreen && offscreen.request->submission == 0 && attempt.status != AttemptStatus::Succeeded)
            {
                const auto reason = attempt.viewportReason != ViewSettingReason::None ? attempt.viewportReason :
                    ViewSettingReason::ScenePreparationFailed;
                offscreen.request->reason = reason;
                static_cast<void>(offscreen.request->view.Unavailable(reason));
            }
            // TryFrame 已结束图回调和 CPU 借用. 后继尝试只含 UI, Draw 强引用仍保留到同槽完成.
            slot.forward.ReleasePayload();
            remember(attempt.error);
            remember(attempt.additional);
            if (attempt.status == AttemptStatus::Stopped || host.HasFatalGraphicsError())
            {
                return stop();
            }
            if (attempt.status == AttemptStatus::GraphRejected &&
                (mode != Editor::UiFrameMode::SafeUi || attemptedOffscreen))
            {
                mode = Editor::UiFrameMode::SafeUi;
                if (attempt.viewportReason != ViewSettingReason::None)
                {
                    static_cast<void>(view.Unavailable(attempt.viewportReason));
                }
                outputs = view.LastAcceptedOutput(attempt.viewportReason);
                continue;
            }
            if (attempt.status == AttemptStatus::RebuildWithoutViewport)
            {
                // 解除本轮场景生产者, 保留准入原因及按同一规则可读的旧图.
                if (mode == Editor::UiFrameMode::Normal)
                {
                    outputs = viewportFallback(attempt.viewportReason, retainOnFailure);
                }
                continue;
            }
            if (attempt.status == AttemptStatus::Succeeded)
            {
                const bool safe = mode == Editor::UiFrameMode::SafeUi &&
                    (!offscreen.request || offscreen.request->submission == 0);
                // 准备值可用不等于已显示. 只有正常合成实际采样且 View 已接受, 才交回导航命中依据.
                editor.SetAcceptedViewport(attempt.viewportPublished ? &prepared : nullptr, view);
                editor.ReportFrameOutcome(safe ? Editor::FrameRecovery::SafeUi : Editor::FrameRecovery::Normal,
                                          primary, additional);
                return safe ? FrameStatus::SafeUi : FrameStatus::Rendered;
            }
            editor.SetAcceptedViewport(nullptr, view);
            // 已发前缀的取得由 ExecuteFrame 消费; 没有票据的剩余普通分支均尚未 Open.
            if (slot.lastSubmission == 0)
            {
                if (auto error = host.CancelPresentation(presentation.token))
                {
                    remember(Platform::DescribeHostError(*error));
                    return stop();
                }
            }
            if (primary)
            {
                editor.ReportFrameOutcome(Editor::FrameRecovery::Cancelled, primary, additional);
            }
            return attempt.status == AttemptStatus::UploadPending ? FrameStatus::UploadPending : FrameStatus::Cancelled;
        }
    }
}
