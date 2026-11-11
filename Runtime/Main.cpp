/// @file
/// @brief 唯一产品入口. 先验证启动参数, 再装配诊断, 宿主和 Editor.
/// 每次成功 poll 发布一个输入批次. 继续的轮次经 Editor 和公共 RHI 合成, 由宿主提交并呈现窗口颜色.

#include <array>
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
#include <DeviceHost.hpp>
#include <Session/EditorSession.hpp>

#include "Resources.hpp"

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
        Platform::HostDesc host;
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

    // 取得窗口目标, 在一份即时列表上初始化底色并录制封存的 UI, 再按该次录制确认上传和交换.
    // 失败时保留原始原因, 已发前缀交宿主提交或在退出时收拢.
    static std::optional<Core::BoundedText> RenderFrame(Platform::DeviceHost &host, Editor::EditorSession &editor,
                                                        RHI::CommandList &commands, Core::Diagnostics &diagnostics)
    {
        auto acquired = host.BeginPresentation();
        if (const auto *error = std::get_if<Platform::HostError>(&acquired))
        {
            return Platform::DescribeHostError(*error);
        }
        auto *presentation = std::get_if<Platform::PresentationFrame>(&acquired);
        if (!presentation)
        {
            // 暂停和槽忙均由下一轮事件推进. 本轮没有可供录制或提交的目标.
            return std::nullopt;
        }
        // 先准备拥有型 UI 批. 此阶段失败可在打开列表和发出首个写入前退还取得.
        auto ui = editor.PrepareUiFrame(presentation->target.framebuffer, presentation->target.extentPixels);
        if (const auto *error = std::get_if<Core::BoundedText>(&ui))
        {
            static_cast<void>(host.CancelPresentation(presentation->token));
            return *error;
        }
        auto *prepared = std::get_if<Editor::PreparedUiFrame>(&ui);
        const auto errors = host.CaptureRhiErrors();
        std::optional<Core::BoundedText> failure;
        commands.Open();
        if (!errors.IsClear())
        {
            return Core::BoundedText("Cannot open the window Graphics list");
        }
        // 每个有效窗口帧都完整清除目标. 底色按线性值传入, 由 sRGB 附件编码, 空 UI 批也可清屏呈现.
        commands.ClearTextureFloat(presentation->target.texture, RHI::AllSubresources,
                                   RHI::Color(0.018f, 0.024f, 0.035f, 1.f));
        if (errors.IsClear() && prepared)
        {
            failure = prepared->Record(commands);
        }
        // Close 恢复导出状态并纳入本次错误区间. 仅完整录制才允许末端源访问.
        commands.Close();
        const bool recorded = errors.IsClear() && !failure;
        if (auto error = host.PrepareSubmission(presentation->token, commands, recorded))
        {
            return failure ? *failure : Platform::DescribeHostError(*error);
        }
        auto submitted = host.SubmitPresentation(presentation->token);
        if (const auto *error = std::get_if<Platform::HostError>(&submitted))
        {
            return failure ? *failure : Platform::DescribeHostError(*error);
        }
        // 非零前缀票据也可能没有有效颜色. 先记录接受, UI 确认仍取决于完整录制和复制.
        const auto receipt = std::get<Platform::PresentationSubmission>(submitted);
        ReportTicket(diagnostics, "Window Graphics submission accepted", receipt.ticket);
        if (prepared)
        {
            static_cast<void>(prepared->Finish({commands, receipt.ticket, recorded && receipt.colorValid}));
        }
        // Present 消费本次取得. 后续窗口变化或交换失败仍保留此前接受的工作及其退休责任.
        std::optional<Platform::HostError> error;
        const auto result = host.Present(presentation->token, error);
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
        return std::nullopt;
    }

    // 终止轮次只消费发布事实. 继续的轮次推进 Editor 和呈现, 暂停及槽忙仍有限等待事件.
    static int RunEditor(Platform::DeviceHost &host, Editor::EditorSession &editor,
                         Core::Diagnostics &diagnostics, std::optional<std::uint64_t> pollLimit)
    {
        std::optional<bool> previousFrame;
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

            // 暂停轮次也可能需要释放光标捕获. 自持请求在下一次 poll 前交给宿主执行.
            const auto frame = editor.Advance(input);
            if (frame.hostRequest)
            {
                const std::span requests(&*frame.hostRequest, 1);
                if (auto error = host.ApplyDeferredRequests(requests))
                {
                    PrintError(Platform::DescribeHostError(*error));
                    return 1;
                }
                // 请求成功后通知会话. 若本轮已恢复交互, 指针基线先于 Wait 分发的新回调入队.
                editor.CompleteCursorRelease();
            }
            // 只记录 CPU 帧就绪状态的变化. 日志写入结果独立于本轮窗口推进结果.
            if (!previousFrame || *previousFrame != frame.hasDrawData)
            {
                static_cast<void>(diagnostics.Write(Core::Severity::Info,
                                                    frame.hasDrawData ? "Editor CPU frame ready" : "Window suspended"));
                previousFrame = frame.hasDrawData;
            }
            if (frame.hasDrawData)
            {
                if (auto error = RenderFrame(host, editor, *commands, diagnostics))
                {
                    PrintError(*error);
                    return 1;
                }
            }
            // Event 证明槽可复用. 帧临时引用释放后, 在 owner context 零超时收集并排空原生删除队列.
            // 暂停和槽忙也推进回收; 原生完成或删除失败后停止新帧, 交显式关闭收拢.
            const auto errors = host.CaptureRhiErrors();
            device.RunGarbageCollection();
            if (!errors.IsClear())
            {
                PrintError(Core::BoundedText("Window Graphics retirement failed"));
                return 1;
            }
            // Editor 在暂停时清理输入, 两种状态都沿宿主有限等待继续接收回调.
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
    // Editor 与 RunEditor 的逻辑列表在宿主关闭前释放. 已发 GPU 工作沿设备物理包收拢.
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
            auto editor = Editor::EditorSession::Create(*host, {*host->GetDevice(), package});
            if (const auto *editorError = std::get_if<Core::BoundedText>(&editor))
            {
                Runtime::PrintError(*editorError);
                exitCode = 1;
            }
            else
            {
                auto &session = std::get<std::unique_ptr<Editor::EditorSession>>(editor);
                exitCode = Runtime::RunEditor(*host, *session, *diagnostics, options.pollLimit);
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
