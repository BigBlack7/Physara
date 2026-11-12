/// @file
/// @brief 唯一产品入口. 先验证启动参数, 再装配诊断, 宿主和 Editor.
/// 会话封存布局后应用领域编辑, 宿主在录制前处理窗口请求.
/// 有界帧槽组织诊断 View 输出和 UI 合成, 由宿主提交并呈现窗口颜色.

#include <array>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include <Core/Diagnostics.hpp>
#include <Core/Error.hpp>
#include <Core/FrameArena.hpp>
#include <DeviceHost.hpp>
#include <Renderer/View.hpp>
#include <Session/EditorSession.hpp>

#include "Resources.hpp"

namespace Physara::Runtime
{
    // 装配时将同一容量传给宿主, 让 CPU arena 与窗口目标使用相同槽索引.
    static constexpr std::size_t FrameSlotCount = Platform::MaxPresentationSlots;

    static constexpr std::string_view Usage =
        "Physara - native window and RHI editor runtime\n"
        "Usage: Physara [--help] [--hidden] [--width=N] [--height=N] [--poll-limit=N]\n"
        "Dimensions are unsigned 32-bit logical units. Poll limit is a positive unsigned 64-bit value.\n"
        "Visible and hidden modes use the same editor session and require a real graphics context.\n"
        "Renderable frames compose the editor through RHI and present the window color.\n";

    /// @brief 通过命令行语法验证的自持启动配置. 原生尺寸约束由宿主检查.
    struct StartupOptions
    {
        Platform::HostDesc host{.presentationSlotCount = FrameSlotCount};
        // 按成功发布的批次数计数, 空值表示由窗口关闭请求决定退出时机.
        std::optional<std::uint64_t> pollLimit;
        bool help = false;
    };

    // 必须消费整个十进制字段, 拒绝空值, 符号, 溢出或尾随字符.
    template <typename Integer>
    static bool ParseUnsigned(std::string_view text, Integer &value) noexcept
    {
        if (text.empty())
        {
            return false;
        }
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    }

    // 逐项拒绝未知/重复参数; help 仍须通过整批语法检查, 宿主负责尺寸的原生范围约束.
    static std::variant<StartupOptions, Core::BoundedText> ParseOptions(int argc, char **argv) noexcept
    {
        // 每种选项占一位以拒绝重复; 所有参数都检查完后, main 才解释 help 或创建原生状态.
        StartupOptions options;
        unsigned seen = 0;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);
            unsigned bit = 0;
            bool valid = true;
            if (argument == "--help")
            {
                bit = 1;
                options.help = true;
            }
            else if (argument == "--hidden")
            {
                bit = 2;
                options.host.visible = false;
            }
            else if (argument.starts_with("--width="))
            {
                bit = 4;
                valid = ParseUnsigned(argument.substr(8), options.host.clientWidthLogical);
            }
            else if (argument.starts_with("--height="))
            {
                bit = 8;
                valid = ParseUnsigned(argument.substr(9), options.host.clientHeightLogical);
            }
            else if (argument.starts_with("--poll-limit="))
            {
                bit = 16;
                std::uint64_t limit = 0;
                valid = ParseUnsigned(argument.substr(13), limit) && limit > 0;
                options.pollLimit = limit;
            }
            else
            {
                valid = false;
            }
            if (!valid || (seen & bit) != 0)
            {
                Core::BoundedText error("Invalid or duplicate startup option: ");
                error.Append(argument);
                return error;
            }
            seen |= bit;
        }
        return options;
    }

    // 最后退路只向 stderr 写有界正文, 不依赖诊断设施已成功构造.
    static void PrintError(const Core::BoundedText &text) noexcept
    {
        // 输出不是错误形成的前置; stderr 失败时仍保留原来的进程结果.
        static_cast<void>(std::fwrite(text.View().data(), 1, text.View().size(), stderr));
        static_cast<void>(std::fwrite("\n", 1, 1, stderr));
    }

    // 同一票据关联提交与交换记录. 写入失败不影响本次 GPU 工作或交换结果.
    static void ReportTicket(Core::Diagnostics &diagnostics, std::string_view event, std::uint64_t ticket) noexcept
    {
        std::array<char, 24> number{};
        const auto converted = std::to_chars(number.data(), number.data() + number.size(), ticket);
        Core::BoundedText message(event);
        message.Append(", ticket=");
        message.Append({number.data(), static_cast<std::size_t>(converted.ptr - number.data())});
        static_cast<void>(diagnostics.Write(Core::Severity::Info, message.View()));
    }

    /// @brief Runtime 的帧内存储和接受回执. lastSubmission 用于关联诊断, 复用以同槽 Ready 为准.
    struct FrameSlot
    {
        Core::FrameArena arena;
        std::uint64_t lastSubmission = 0;
        std::size_t diagnosticUploadBytes = 0; ///< 只为在途诊断上传记账, Ready 后归还额度.
    };

    // 轮询有界的一圈. 仅 Retry 允许检查其它索引, 失败不能误作槽忙或尝试绕过.
    static Platform::DeviceHost::PresentationAcquire AcquireFrame(Platform::DeviceHost &host, std::size_t &next)
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

    /// @brief 本轮帧推进结果. Rendered 包括已提交但因窗口变化跳过交换的帧.
    /// UploadPending 表示开放列表前已归还取得, 后续轮次可重新准备.
    enum class FrameStatus
    {
        Rendered,
        UploadPending
    };

    // 先按封存请求准备颜色或占位, UI 核定本轮采样和共享上传准入后再开放列表.
    // 录制失败时保留原始原因, 已发前缀交同槽提交或退出收拢.
    static std::variant<FrameStatus, Core::BoundedText> RenderFrame(
        Platform::DeviceHost &host, Editor::EditorSession &editor, Renderer::View &view,
        const Editor::EditorFrame &editorFrame, const Platform::PresentationFrame &presentation, FrameSlot &slot,
        std::size_t remainingDiagnosticBytes, RHI::CommandList &commands, Core::Diagnostics &diagnostics)
    {
        const auto cancel = [&host, &presentation](Core::BoundedText failure)
        {
            static_cast<void>(host.CancelPresentation(presentation.token));
            return failure;
        };
        const auto errors = host.CaptureRhiErrors();
        auto preparedOutputs = view.Prepare(editorFrame.viewport, remainingDiagnosticBytes);
        if (const auto *error = std::get_if<Core::BoundedText>(&preparedOutputs))
        {
            return cancel(*error);
        }
        auto &outputs = std::get<Renderer::NoSceneOutputs>(preparedOutputs);
        auto ui = editor.PrepareUiFrame(presentation.target.framebuffer, presentation.target.extentPixels,
                                        slot.arena, outputs, commands.GetDesc());
        if (const auto *error = std::get_if<Core::BoundedText>(&ui))
        {
            return cancel(*error);
        }
        if (!errors.IsClear())
        {
            return cancel(Core::BoundedText("Window Graphics frame preparation failed"));
        }
        if (std::holds_alternative<Editor::UiPrepareStatus>(ui))
        {
            if (outputs.Diagnostic() || outputs.UploadPending())
            {
                view.Defer(Renderer::ViewSettingReason::UploadPending);
            }
            // 当前取得尚无新命令. 归还后继续处理输入和退休, 后续轮次再按当时请求准备.
            if (const auto error = host.CancelPresentation(presentation.token))
            {
                return Platform::DescribeHostError(*error);
            }
            return FrameStatus::UploadPending;
        }

        auto *prepared = std::get_if<Editor::PreparedUiFrame>(&ui);
        const bool useDiagnostic = prepared && prepared->UsesDiagnosticOutput();
        if (prepared && prepared->DiagnosticUploadPending())
        {
            view.Defer(Renderer::ViewSettingReason::UploadPending);
        }
        // 仅生成本次实际采样的诊断像素. UI 已将其写入计入准入及 arena 预留.
        if (useDiagnostic && !outputs.PrepareUpload(slot.arena))
        {
            return cancel(Core::BoundedText("Window Graphics frame preparation failed"));
        }
        std::optional<Core::BoundedText> failure;
        commands.Open();
        if (!errors.IsClear())
        {
            return Core::BoundedText("Cannot open the window Graphics list");
        }
        // 先写入本帧采样的诊断候选. 窗口底色独立初始化, 在 sRGB 附件上存储编码.
        if (useDiagnostic)
        {
            outputs.RecordUpload(commands);
            slot.diagnosticUploadBytes = outputs.UploadBytes();
        }
        if (errors.IsClear())
        {
            commands.ClearTextureFloat(presentation.target.texture, RHI::AllSubresources,
                                       RHI::Color(0.018f, 0.024f, 0.035f, 1.f));
        }
        if (errors.IsClear() && prepared)
        {
            failure = prepared->Record(commands);
        }
        // Close 恢复导出状态并纳入本次错误区间. 仅完整录制才允许末端源访问.
        commands.Close();
        const bool recorded = errors.IsClear() && !failure;
        if (auto error = host.PrepareSubmission(presentation.token, commands, recorded))
        {
            return failure ? *failure : Platform::DescribeHostError(*error);
        }
        std::optional<Platform::HostError> error;
        const auto receipt = host.SubmitPresentation(presentation.token, error);
        // Event 设置或交换稍后失败也不改变设备已接受的事实.
        slot.lastSubmission = receipt.ticket;
        if (slot.lastSubmission != 0)
        {
            ReportTicket(diagnostics, "Window Graphics submission accepted", slot.lastSubmission);
        }
        if (error)
        {
            return failure ? *failure : Platform::DescribeHostError(*error);
        }
        // 前缀也可获非零票据. 仅通过颜色资格的完整录制确认纹理请求和候选 View 颜色.
        if (prepared)
        {
            const bool confirmed = prepared->Finish({commands, receipt.ticket, recorded && receipt.colorValid});
            assert(confirmed == receipt.colorValid);
            static_cast<void>(confirmed);
        }
        if (receipt.colorValid && useDiagnostic)
        {
            // 有效比例随本轮采样到的颜色一起确认. 同尺寸复用也经过这次提交.
            view.Publish(std::move(outputs));
        }
        // Present 消费本次取得. 后续窗口变化或交换失败仍保留此前接受的工作及其退休责任.
        const auto result = host.Present(presentation.token, error);
        if (failure)
        {
            return *failure;
        }
        if (!recorded)
        {
            return Core::BoundedText("Window Graphics recording failed");
        }
        if (error)
        {
            return Platform::DescribeHostError(*error);
        }
        if (result.swapped)
        {
            ReportTicket(diagnostics, "Editor window presented", result.ticket);
        }
        return FrameStatus::Rendered;
    }

    // 标题与 VSync 在同一 CPU 边界各自应用. 字段拒绝保留原因并继续尝试另一字段.
    // 宿主生命周期错误结束运行, 由调用者归还尚未录制的窗口取得.
    static std::optional<Platform::HostError> ApplyWindowSettings(Platform::DeviceHost &host,
                                                                  const Editor::EditorFrame &frame) noexcept
    {
        if (frame.titleRequest)
        {
            if (auto error = host.ApplyWindowTitle(*frame.titleRequest);
                error && error->reason != Platform::HostErrorReason::RequestRejected)
            {
                return error;
            }
        }
        if (frame.vsyncRequest)
        {
            if (auto error = host.ApplyWindowVSync(*frame.vsyncRequest);
                error && error->reason != Platform::HostErrorReason::RequestRejected)
            {
                return error;
            }
        }
        return std::nullopt;
    }

    // 终止轮次只消费发布事实. 继续的轮次推进 Editor 和呈现, 暂停及槽忙仍有限等待事件.
    static int RunEditor(Platform::DeviceHost &host, Editor::EditorSession &editor, Renderer::View &view,
                         Core::Diagnostics &diagnostics, std::optional<std::uint64_t> pollLimit)
    {
        std::optional<bool> previousFrame;
        std::array<FrameSlot, FrameSlotCount> slots;
        std::size_t nextSlot = 0;
        bool wasSlotBusy = false;
        bool wasUploadPending = false;
        // 跨帧重用逻辑列表. 每次 Open 分配本轮物理包, 由已接受的提交接管在途引用.
        auto &device = *host.GetDevice();
        auto commands = device.CreateCommandList();
        if (!commands)
        {
            PrintError(Core::BoundedText("Cannot create the product Graphics list"));
            return 1;
        }
        for (;;)
        {
            auto polled = host.PollEvents();
            if (const auto *error = std::get_if<Platform::HostError>(&polled))
            {
                PrintError(Platform::DescribeHostError(*error));
                return 1;
            }
            // 计数取自宿主发布, Wait 中到达的回调并入下一批. 先判退出可避免额外推进一帧.
            const auto &input = std::get<Platform::HostFrameInput>(polled);
            if (input.window.closeRequested || (pollLimit && input.sequence >= *pollLimit))
            {
                return 0;
            }

            auto acquired = input.window.Renderable() ? AcquireFrame(host, nextSlot) :
                            Platform::DeviceHost::PresentationAcquire{Platform::PresentationStatus::Suspended};
            if (const auto *error = std::get_if<Platform::HostError>(&acquired))
            {
                PrintError(Platform::DescribeHostError(*error));
                return 1;
            }
            const auto *status = std::get_if<Platform::PresentationStatus>(&acquired);
            const bool slotBusy = status && *status == Platform::PresentationStatus::Retry;
            if (slotBusy && !wasSlotBusy)
            {
                static_cast<void>(diagnostics.Write(Core::Severity::Info, "FrameSlot backpressure: all 3 slots pending"));
            }
            wasSlotBusy = slotBusy;
            const auto *presentation = std::get_if<Platform::PresentationFrame>(&acquired);

            // CPU 仍在槽忙时推进. Advance 封存 UI 并应用领域编辑, 窗口请求随后在录制前处理.
            const auto frame = editor.Advance(input, &view, host.GetWindowSettings());
            if (frame.hostRequest)
            {
                const std::span requests(&*frame.hostRequest, 1);
                if (auto error = host.ApplyDeferredRequests(requests))
                {
                    if (presentation)
                    {
                        static_cast<void>(host.CancelPresentation(presentation->token));
                    }
                    PrintError(Platform::DescribeHostError(*error));
                    return 1;
                }
                // 请求成功后通知会话. 若本轮已恢复交互, 指针基线先于 Wait 分发的新回调入队.
                editor.CompleteCursorRelease();
            }
            if (auto error = ApplyWindowSettings(host, frame))
            {
                if (presentation)
                {
                    static_cast<void>(host.CancelPresentation(presentation->token));
                }
                PrintError(Platform::DescribeHostError(*error));
                return 1;
            }
            // 只记录 CPU 帧就绪状态的变化. 日志写入结果独立于本轮窗口推进结果.
            if (!previousFrame || *previousFrame != frame.hasDrawData)
            {
                static_cast<void>(diagnostics.Write(Core::Severity::Info,
                                                    frame.hasDrawData ? "Editor CPU frame ready" : "Window suspended"));
                previousFrame = frame.hasDrawData;
            }
            if (presentation)
            {
                if (!frame.hasDrawData)
                {
                    static_cast<void>(host.CancelPresentation(presentation->token));
                    PrintError(Core::BoundedText("Editor did not seal a CPU frame for the acquired window target"));
                    return 1;
                }
                // Ready 已证明这一个槽可以覆写. Pending 或失败从未取得以下存储的使用权.
                auto &slot = slots[presentation->token.slotIndex];
                slot.arena.Reset();
                slot.lastSubmission = 0;
                slot.diagnosticUploadBytes = 0;
                // 这里只应用诊断图的独立预算. 实际共享上传区段由准备阶段向设备预检.
                auto remaining = Renderer::DiagnosticUploadBudget;
                for (const auto &pending : slots)
                {
                    remaining -= pending.diagnosticUploadBytes;
                }
                const auto rendered =
                    RenderFrame(host, editor, view, frame, *presentation, slot, remaining, *commands, diagnostics);
                if (const auto *error = std::get_if<Core::BoundedText>(&rendered))
                {
                    PrintError(*error);
                    return 1;
                }
                const bool uploadPending = std::get<FrameStatus>(rendered) == FrameStatus::UploadPending;
                if (uploadPending && !wasUploadPending)
                {
                    static_cast<void>(diagnostics.Write(Core::Severity::Info,
                                                        "Graphics upload backpressure: waiting for retired storage"));
                }
                wasUploadPending = uploadPending;
            }
            else
            {
                view.Defer(slotBusy && frame.viewport ? Renderer::ViewSettingReason::SlotPending :
                                                         Renderer::ViewSettingReason::LayoutPending);
            }
            // 帧快照持有的 framebuffer 在原生回收前释放, 以便及时排空被替换目标的删除记录.
            acquired = Platform::PresentationStatus::Suspended;
            // Event 证明槽可复用. 帧临时引用释放后, 在 owner context 零超时收集并排空原生删除队列.
            // 窗口暂停, 槽忙和上传背压都推进回收. 完成观察或删除失败后交显式关闭收拢.
            const auto errors = host.CaptureRhiErrors();
            device.RunGarbageCollection();
            if (!errors.IsClear())
            {
                PrintError(Core::BoundedText("Window Graphics retirement failed"));
                return 1;
            }
            // 有限等待把收到的回调留给下一次 poll. 正常, 槽忙及暂停轮次共用这条入口.
            if (auto error = host.WaitForEvents(0.05))
            {
                PrintError(Platform::DescribeHostError(*error));
                return 1;
            }
        }
    }
}

// 退出码: 参数错误为 2, 初始化/运行/关闭失败为 1, 帮助或完整正常关闭为 0.
int main(int argc, char **argv)
{
    using namespace Physara;
    auto parsed = Runtime::ParseOptions(argc, argv);
    if (const auto *error = std::get_if<Core::BoundedText>(&parsed))
    {
        Runtime::PrintError(*error);
        return 2;
    }
    const auto &options = std::get<Runtime::StartupOptions>(parsed);
    if (options.help)
    {
        static_cast<void>(std::fwrite(Runtime::Usage.data(), 1, Runtime::Usage.size(), stdout));
        return 0;
    }
    auto diagnosticsResult = Core::Diagnostics::Create();
    if (const auto *error = std::get_if<Core::DiagnosticFailure>(&diagnosticsResult))
    {
        Runtime::PrintError(error->context.description);
        return 1;
    }
    auto diagnostics = std::move(std::get<std::unique_ptr<Core::Diagnostics>>(diagnosticsResult));
    auto creation = Platform::DeviceHost::Create(options.host, *diagnostics);
    if (const auto *error = std::get_if<Platform::HostError>(&creation))
    {
        Runtime::PrintError(Platform::DescribeHostError(*error));
        return 1;
    }
    auto host = std::move(std::get<std::unique_ptr<Platform::DeviceHost>>(creation));
    // Editor, View 和逻辑列表在宿主关闭前释放. 已发 GPU 工作由原物理包保留到设备收拢.
    int exitCode = 0;
    try
    {
        auto located = Runtime::LocateEditorUiPackage();
        if (const auto *error = std::get_if<Core::BoundedText>(&located))
        {
            Runtime::PrintError(*error);
            exitCode = 1;
        }
        else
        {
            const auto &package = std::get<std::filesystem::path>(located);
            auto &device = *host->GetDevice();
            auto view = Renderer::View::Create(device, host->GetInfo().maxTextureSizePixels);
            if (!view)
            {
                Runtime::PrintError(Core::BoundedText("Cannot allocate the main view identity"));
                exitCode = 1;
            }
            else
            {
                auto editor = Editor::EditorSession::Create(*host, {device, package, view->Id()});
                if (const auto *editorError = std::get_if<Core::BoundedText>(&editor))
                {
                    Runtime::PrintError(*editorError);
                    exitCode = 1;
                }
                else
                {
                    auto &session = std::get<std::unique_ptr<Editor::EditorSession>>(editor);
                    exitCode = Runtime::RunEditor(*host, *session, *view, *diagnostics, options.pollLimit);
                }
            }
        }
    }
    catch (const std::exception &error)
    {
        Runtime::PrintError(Core::BoundedText(error.what()));
        exitCode = 1;
    }
    catch (...)
    {
        Runtime::PrintError(Core::BoundedText("Unknown runtime failure"));
        exitCode = 1;
    }
    // 诊断 owner 在宿主关闭期间仍可用. 运行错误已先报告, 此处单独记录关闭错误.
    if (auto error = host->Shutdown())
    {
        Runtime::PrintError(Platform::DescribeHostError(*error));
        exitCode = 1;
        // 排空未完成时 owner 和 context 仍存活. 沿宿主协议重试, 保留第一次关闭失败的结果.
        while (error && error->reason == Platform::HostErrorReason::RhiDrainPending)
        {
            error = host->Shutdown();
        }
    }
    return exitCode;
}
