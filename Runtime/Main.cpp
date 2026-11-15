/// @file
/// @brief 唯一产品入口. 先验证启动参数, 再装配诊断, 宿主和 Editor.
/// 会话封存布局后应用领域编辑, 宿主在录制前处理窗口请求.
/// 有界帧槽持有载荷和图绑定, 同一张图组织上传与 UI 合成, 由宿主提交并呈现窗口颜色.

#include <algorithm>
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
#include "Frame.hpp"

namespace Physara::Runtime
{
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
            if (host.HasFatalGraphicsError())
            {
                PrintError(host.FirstGraphicsError().value_or(Core::BoundedText("Fatal graphics diagnostic")));
                return 1;
            }
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
            if (host.HasFatalGraphicsError())
            {
                PrintError(host.FirstGraphicsError().value_or(Core::BoundedText("Fatal UI preparation")));
                return 1;
            }
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
                // 这里只应用诊断图的独立预算. 实际共享上传区段由准备阶段向设备预检.
                auto remaining = Renderer::DiagnosticUploadBudget;
                for (const auto &pending : slots)
                {
                    if (&pending != &slot)
                    {
                        remaining -= pending.diagnosticUploadBytes;
                    }
                }
                const auto rendered =
                    RenderFrame(host, editor, view, frame, *presentation, slot, remaining, *commands, diagnostics);
                if (const auto *error = std::get_if<Core::BoundedText>(&rendered))
                {
                    PrintError(*error);
                    return 1;
                }
                // SafeUi 和 Cancelled 都已处理本次取得. 保持同一事件/退休节奏, 不把已处理帧错误当成退出失败.
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
            auto view = Renderer::View::Create(host->GetInfo().maxTextureSizePixels);
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
