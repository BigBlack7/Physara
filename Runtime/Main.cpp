/// @file
/// @brief 唯一产品入口. 先验证启动参数, 再装配诊断, 宿主和 Editor.
/// 每次成功 poll 发布一个输入批次. Runtime 据此选择退出或推进 CPU UI 帧.

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

namespace Physara::Runtime
{
    static constexpr std::string_view Usage =
        "Physara - native window and CPU editor runtime\n"
        "Usage: Physara [--help] [--hidden] [--width=N] [--height=N] [--poll-limit=N]\n"
        "Dimensions are unsigned 32-bit logical units. Poll limit is a positive unsigned 64-bit value.\n"
        "Visible and hidden modes use the same editor session and require a real graphics context.\n"
        "This stage builds CPU UI frames. Window drawing and presentation are pending.\n";

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

    // 终止轮次只消费发布事实. 继续的轮次先推进 Editor, 再应用请求并有限等待.
    static int RunEditor(Platform::DeviceHost &host, Editor::EditorSession &editor,
                         Core::Diagnostics &diagnostics, std::optional<std::uint64_t> pollLimit)
    {
        std::optional<bool> previousFrame;
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
    // Editor 的寿命止于 try 块. 正常返回和异常展开都先拆除输入适配, 再显式关闭宿主.
    int exitCode = 0;
    try
    {
        auto editor = Editor::EditorSession::Create(*host);
        if (const auto *error = std::get_if<Core::BoundedText>(&editor))
        {
            Runtime::PrintError(*error);
            exitCode = 1;
        }
        else
        {
            auto &session = std::get<std::unique_ptr<Editor::EditorSession>>(editor);
            exitCode = Runtime::RunEditor(*host, *session, *diagnostics, options.pollLimit);
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
    }
    return exitCode;
}
