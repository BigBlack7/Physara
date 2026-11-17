#include "Frame.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

/// @file
/// @brief 分别完成候选准备, 图声明/编译和原列表执行, 在明确边界选择恢复或退出.
/// 失败尝试销毁后才重建图; 一旦 Open, 只处理原前缀而不追加恢复命令.
/// Scene/View 在候选循环前准备一次, 图尝试只消费准备结果及各 owner 已持有的依赖.

namespace Physara::Runtime
{
    using namespace Renderer;

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

    /// @brief 本候选的能力存储, options 的 span 只借用到当前编译返回.
    struct EditorGraphProfile
    {
        std::array<GraphTextureSupport, 4> textures;
        std::array<RHI::ResourceStates, 2> states{RHI::ResourceStates::Common, RHI::ResourceStates::ShaderResource};
        GraphCompileOptions options;

        // 基础 UI 和场景候选使用各自的格式集合, 不把未启用路径或 RHI 未实现的能力交给编译器.
        // options 的借用依赖本对象地址稳定, 因此在 TryFrame 的同一局部作用域内构造并消费.
        explicit EditorGraphProfile(Platform::DeviceHost &host, bool scene)
        {
            const auto info = host.GetInfo();
            constexpr std::array formats{RHI::Format::SRGBA8_UNORM, RHI::Format::RGBA8_UNORM,
                                         RHI::Format::RGBA16_FLOAT, RHI::Format::D32};
            const auto count = scene ? formats.size() : 2;
            for (std::size_t index = 0; index < count; ++index)
            {
                textures[index] = {.format = formats[index], .dimension = RHI::TextureDimension::Texture2D,
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
            options = {.textures = std::span(textures).first(count),
                .buffers = {.maxBytes = scene ? ForwardBufferLimit : 8 * 1024 * 1024,
                            .maxConstantBytes = RHI::ConstantBufferOffsetSizeAlignment,
                            .constantOffsetAlignment = info.uniformBufferOffsetAlignmentBytes},
                .textureBoundaryStates = states, .bufferBoundaryStates = states, .maxColorAttachments = 1,
                .transientByteBudget = (scene ? ForwardAttachmentBudget : DiagnosticUploadBudget) + 32 * 1024 * 1024 + 4};
        }
    };

    /// @brief 尝试结果只有一种动作. GraphRejected 才允许调用方在无录制阶段建立安全图.
    enum class AttemptStatus { Succeeded, RebuildWithoutViewport, UploadPending, GraphRejected, Cancelled, Stopped };

    /// @brief 一张候选图的最终动作与拥有型失败摘要, 返回后不再依赖该图或准备值.
    /// error 保留该候选首错, additional 说明后续收拢为何失败; 票据另记在原帧槽中.
    struct AttemptResult
    {
        AttemptStatus status;
        std::optional<Core::BoundedText> error;
        std::optional<Core::BoundedText> additional;
        ViewSettingReason viewportReason = ViewSettingReason::None; ///< 仅重建占位时交给 View, 区分等待和硬拒绝.
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
        std::optional<ForwardOutput> scene;
        PassId diagnosticUpload;
        std::vector<GraphUpload> uploads = std::vector<GraphUpload>(0);
    };

    // 正常和安全 UI 共用声明路径. 旧输出只 import, 安全模式不会生成诊断像素或附带消费者.
    static GraphResult<FrameDeclaration> DeclareFrame(RenderGraph &graph, const Platform::PresentTarget &target,
        PreparedViewOutput &outputs, Editor::PreparedUiFrame *prepared, Editor::UiFrameMode mode,
        FrameSlot &slot, GraphExecutionDiagnostics diagnostics)
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
        if (outputs.Color() && prepared && prepared->UsesViewOutput())
        {
            if (outputs.Color()->scene && mode == Editor::UiFrameMode::Normal)
            {
                // 设备准入按实际纹理、缓冲顺序记账. UI 纹理须在场景缓冲写前声明.
                if (auto error = prepared->DeclareTextureUploads(graph, declared.color, slot.bindings, declared.uploads))
                {
                    return *error;
                }
                auto forward = slot.forward.Declare(graph, declared.uploads);
                if (const auto *error = std::get_if<GraphError>(&forward))
                {
                    return *error;
                }
                declared.scene = std::get<ForwardOutput>(std::move(forward));
                declared.viewport = declared.scene->displayColor;
            }
            else
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
        return declared;
    }

    // 成功 Close 恢复 keepInitialState. 失败目标将换代, 其它持久导入必须仍满足下次入口事实.
    // 不推测未执行到哪个 Pass, 按整个存活计划保守检查可能已消费的导入.
    // 这里只证明状态可重新进入, 不撤销已经发生的像素更新; 未确认的 UI 请求由其原 owner 重试.
    static bool ImportsCanRetry(const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Texture *target) noexcept
    {
        for (const auto &texture : plan.textures)
        {
            const auto &source = graph.Textures()[texture.resource.identity.index];
            if (texture.importState && source.imported.Get() != target &&
                (!source.desc.keepInitialState || source.desc.initialState != texture.importState->entry))
            {
                return false;
            }
        }
        for (const auto &buffer : plan.buffers)
        {
            const auto &source = graph.Buffers()[buffer.resource.identity.index];
            if (buffer.importState &&
                (!source.desc.keepInitialState || source.desc.initialState != buffer.importState->entry))
            {
                return false;
            }
        }
        return true;
    }

    // Open 后所有失败留在原录制, 依次完成图执行, Close, 提交, 发布资格核对和取得消费.
    // Close/交接区间独立检查, 但不能清除原图的失败结论. 无可靠票据时保持原 owner 等待退出收拢.
    static AttemptResult ExecuteFrame(Platform::DeviceHost &host, Editor::EditorSession &editor,
        View &view, const Platform::PresentationFrame &presentation, FrameSlot &slot, RHI::CommandList &commands,
        Core::Diagnostics &diagnostics, RenderGraph &graph, const CompiledGraphPlan &plan,
        const FrameDeclaration &declared, PreparedViewOutput &outputs, Editor::PreparedUiFrame *prepared,
        Editor::UiFrameMode mode, const Platform::RhiErrorScope &errors, GraphExecutionDiagnostics graphErrors,
        bool &opened)
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
        auto executed = ExecuteRenderGraph(graph, plan, *host.GetDevice(), commands, graphErrors);
        if (const auto *error = std::get_if<GraphError>(&executed))
        {
            failure = Failed(host, AttemptStatus::Cancelled, "Frame.Execute", error->description).error;
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
        if (receipt.ticket != 0)
        {
            ReportTicket(diagnostics, "Window Graphics submission accepted", receipt.ticket);
        }
        if (hostError || !handoffErrors.IsClear() || receipt.ticket == 0)
        {
            return stopped("Frame.Submit", hostError ? Platform::DescribeHostError(*hostError) :
                                                       Core::BoundedText("Submission has no reliable receipt"));
        }
        const bool accepted = recorded && receipt.colorValid;
        const bool usedViewport = prepared && prepared->UsesViewOutput();
        // 确认只消费当前图实际执行的 UI 请求. 安全图可以确认自己的上传, 但永不发布旧 View 快照.
        if (prepared)
        {
            const bool confirmed = prepared->Finish({commands, plan, execution.get(), receipt.ticket, accepted});
            assert(confirmed == accepted);
            static_cast<void>(confirmed);
        }
        const bool published = accepted && usedViewport && mode == Editor::UiFrameMode::Normal &&
            view.Publish(std::move(outputs), RHI::TextureHandle(FindTextureOutput(*execution, declared.viewport)));
        if (published && declared.scene)
        {
            // 先确认同一候选确实成为 View 输出, 再允许其静态上传复用. 前缀和安全图均不进入此处.
            slot.forward.Confirm(*declared.scene, plan);
        }
        const auto result = host.Present(presentation.token, hostError);
        if (hostError)
        {
            return stopped("Frame.Present", Platform::DescribeHostError(*hostError));
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

    // 先准备 UI, 再声明/封存/编译图, 只按存活上传账单准入后 Open.
    // 失败返回按局部对象逆序结束计划, 图和准备值, 调用者随后才可 reset 槽.
    // 宿主首错捕获持续到下一候选或 Shutdown; 本次错误须在返回前复制到 AttemptResult.
    static AttemptResult TryFrame(Platform::DeviceHost &host, Editor::EditorSession &editor, View &view,
        const Platform::PresentationFrame &presentation, FrameSlot &slot, RHI::CommandList &commands,
        Core::Diagnostics &diagnostics, PreparedViewOutput &outputs, Editor::UiFrameMode mode)
    {
        bool opened = false;
        try
        {
            const auto errors = host.BeginFrameGraphicsDiagnostics();
            const GraphExecutionDiagnostics graphErrors{&errors, +[](const void *scope) noexcept
            {
                return static_cast<const Platform::RhiErrorScope *>(scope)->IsClear();
            }};
            const auto preparationFailure = editor.BaseFontReady() ? AttemptStatus::Cancelled : AttemptStatus::Stopped;
            auto ui = editor.PrepareUiFrame(presentation.target.extentPixels, slot.arena, outputs, mode);
            if (const auto *error = std::get_if<Core::BoundedText>(&ui))
            {
                return Failed(host, preparationFailure, "Frame.UiPrepare", *error);
            }
            if (!errors.IsClear())
            {
                return Failed(host, preparationFailure, "Frame.UiPrepare", Core::BoundedText("UI preparation failed"));
            }
            auto *prepared = std::get_if<Editor::PreparedUiFrame>(&ui);
            const auto sceneBytes = mode == Editor::UiFrameMode::Normal && outputs.Color() && outputs.Color()->scene &&
                prepared && prepared->UsesViewOutput() ? slot.forward.GraphStorageBytes() : 0;
            if (!slot.graphArena.Reserve((prepared ? prepared->GraphStorageBytes() : 0) + 1024 + sceneBytes))
            {
                return Failed(host, preparationFailure, "Frame.Prepare",
                              Core::BoundedText("Cannot reserve graph storage"));
            }
            RenderGraph graph(slot.graphArena);
            auto declaration = DeclareFrame(graph, presentation.target, outputs, prepared, mode, slot, graphErrors);
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
            const auto &declared = std::get<FrameDeclaration>(declaration);
            EditorGraphProfile profile(host, declared.scene.has_value());
            if (!errors.IsClear())
            {
                return Failed(host, preparationFailure, "Frame.Profile",
                              Core::BoundedText("Cannot prepare graph capabilities"));
            }
            auto compiled = CompileRenderGraph(graph, profile.options);
            if (const auto *error = std::get_if<GraphError>(&compiled))
            {
                return Failed(host, AttemptStatus::GraphRejected, "Frame.Compile", error->description);
            }
            const auto &plan = *std::get<std::unique_ptr<const CompiledGraphPlan>>(compiled);
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
            if ((admission == Platform::UploadAdmission::Pending ||
                 admission == Platform::UploadAdmission::ExceedsLimit) &&
                mode == Editor::UiFrameMode::Normal && ((declared.scene && declared.scene->HasLiveUploads(plan)) ||
                                                        IsLivePass(plan, declared.diagnosticUpload)))
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
            return ExecuteFrame(host, editor, view, presentation, slot, commands, diagnostics,
                                graph, plan, declared, outputs, prepared, mode, errors, graphErrors, opened);
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

    // 一次取得最多尝试带视口的普通图, 去掉视口的普通图和安全 UI 图.
    // 去掉视口后仍为 Normal, 但空颜色阻止再次进入视口上传分支; SafeUi 不返回普通图.
    // 可恢复分支在此消费取得或沿已提交前缀返回; Stopped 将尚未收拢的 owner 留给主循环退出.
    std::variant<FrameStatus, Core::BoundedText> RenderFrame(
        Platform::DeviceHost &host, Editor::EditorSession &editor, View &view, ForwardRenderer &forward,
        const Editor::EditorFrame &editorFrame, const Platform::PresentationFrame &presentation, FrameSlot &slot,
        std::size_t remainingDiagnosticBytes, RHI::CommandList &commands, Core::Diagnostics &diagnostics)
    {
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
        slot.forward.Reset();
        const auto *scene = editor.ActiveScene();
        assert(scene);
        const auto prepared = slot.mainView.Prepare(slot.sceneFrame, *scene, view, editorFrame.viewport);
        editor.SetPreparedViewport(&prepared);
        // 局部 View 失败仍进入诊断占位和基础 UI, 这里记录原因而不撤销其它可用 UI.
        remember(prepared.failure);
        std::variant<PreparedViewOutput, Core::BoundedText> candidate;
        const bool hasScene = prepared.camera && std::ranges::any_of(prepared.scene.primitives,
            [](const PrimitiveRef &primitive) { return primitive.bounds.has_value(); });
        if (hasScene)
        {
            const auto errors = host.BeginFrameGraphicsDiagnostics();
            auto failure = forward.Prepare(slot.forward, prepared, view.Preview());
            if (!failure && !errors.IsClear())
            {
                failure = ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                                         Core::BoundedText("Forward resource preparation failed")};
            }
            if (failure)
            {
                remember(Failed(host, AttemptStatus::Cancelled, "Frame.ScenePrepare", failure->description).error);
                candidate = view.Unavailable(failure->reason);
            }
            else
            {
                candidate = view.PreparePreview(prepared, slot.forward.Degradation());
            }
            if (host.HasFatalGraphicsError())
            {
                return stop();
            }
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
        // 本次取得已证明同槽上次工作完成. 后续失败分支只检查本轮是否真的提交过前缀.
        slot.lastSubmission = 0;
        slot.diagnosticUploadBytes = 0;
        auto mode = Editor::UiFrameMode::Normal;
        for (;;)
        {
            // 首轮已由同槽 Ready 证明安全; 后继候选只会在无录制且旧尝试完全析构后到达.
            // 图 arena 已由 RenderGraph 析构回退; 这里重置独立载荷区和仅属于旧图的 token 绑定.
            slot.arena.Reset();
            slot.bindings.Reset();
            const auto attempt = TryFrame(host, editor, view, presentation, slot, commands, diagnostics, outputs, mode);
            // TryFrame 已结束图回调和 CPU 借用. 后继尝试只含 UI, Draw 强引用仍保留到同槽完成.
            slot.forward.ReleasePayload();
            remember(attempt.error);
            remember(attempt.additional);
            if (attempt.status == AttemptStatus::Stopped || host.HasFatalGraphicsError())
            {
                return stop();
            }
            if (attempt.status == AttemptStatus::GraphRejected && mode != Editor::UiFrameMode::SafeUi)
            {
                mode = Editor::UiFrameMode::SafeUi;
                outputs = view.LastAcceptedOutput();
                continue;
            }
            if (attempt.status == AttemptStatus::RebuildWithoutViewport)
            {
                // 普通 UI 沿同一原因合成占位. 无可发布的颜色, 因此不会重复进入视口上传分支.
                outputs = view.Unavailable(attempt.viewportReason);
                continue;
            }
            if (attempt.status == AttemptStatus::Succeeded)
            {
                const bool safe = mode == Editor::UiFrameMode::SafeUi;
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
