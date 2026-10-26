#include <charconv>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include <Core/Diagnostics.hpp>
#include <Core/Error.hpp>
#include <DeviceHost.hpp>

namespace Physara::Runtime
{
    static constexpr std::string_view Usage =
        "Physara - native window event runtime\n"
        "Usage: Physara [--help] [--hidden] [--width=N] [--height=N] [--poll-limit=N]\n"
        "Dimensions are unsigned 32-bit logical units. Poll limit is a positive unsigned 64-bit value.\n"
        "The current runtime has no renderer or editor. Hidden mode still requires a real graphics context.\n";

    struct StartupOptions
    {
        Platform::HostDesc host;
        std::optional<std::uint64_t> pollLimit;
        bool help = false;
    };

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

    static std::variant<StartupOptions, Core::BoundedText> ParseOptions(int argc, char **argv) noexcept
    {
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

    static void PrintError(const Core::BoundedText &text) noexcept
    {
        // 输出不是错误形成的前置; stderr 失败时仍保留原来的进程结果.
        static_cast<void>(std::fwrite(text.View().data(), 1, text.View().size(), stderr));
        static_cast<void>(std::fwrite("\n", 1, 1, stderr));
    }
}

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
    int exitCode = 0;
    try
    {
        std::optional<bool> previousRenderable;
        for (;;)
        {
            auto polled = host->PollEvents();
            if (const auto *error = std::get_if<Platform::HostError>(&polled))
            {
                Runtime::PrintError(Platform::DescribeHostError(*error));
                exitCode = 1;
                break;
            }
            const auto &frame = std::get<Platform::HostFrameInput>(polled);
            if (frame.window.closeRequested || (options.pollLimit && frame.sequence >= *options.pollLimit))
            {
                break;
            }
            const bool renderable = frame.window.Renderable();
            if (!previousRenderable || *previousRenderable != renderable)
            {
                static_cast<void>(diagnostics->Write(Core::Severity::Info,
                                                     renderable ? "Window renderable; bootstrap has no graphics work"
                                                                : "Window suspended"));
                previousRenderable = renderable;
            }
            // 当前没有 Editor/Renderer 请求生产者; 正常和暂停状态都等待事件, 不制造空渲染分支.
            if (auto error = host->WaitForEvents(0.05))
            {
                Runtime::PrintError(Platform::DescribeHostError(*error));
                exitCode = 1;
                break;
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
    if (auto error = host->Shutdown())
    {
        Runtime::PrintError(Platform::DescribeHostError(*error));
        exitCode = 1;
    }
    return exitCode;
}
