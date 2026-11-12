#include "DeviceHostInternal.hpp"

/// @file
/// @brief 在宿主的请求边界应用标题和交换区间, 分别用 Win32 与 WGL 结果确认.
/// 每个字段单独保留最新合法请求, 最后确认值及拒绝原因.

#include <bit>
#include <limits>
#include <string>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>

namespace Physara::Platform::Detail
{
    /// @brief WGL_EXT_swap_control 读回入口的原生调用约定.
    using QuerySwapInterval = int (WINAPI *)();

    // 用 Win32 严格解码得到供读回比较的 UTF-16, 空标题也是合法值.
    static std::optional<std::wstring> DecodeTitle(std::string_view title)
    {
        if (title.empty())
        {
            return std::wstring{};
        }
        if (title.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() - 2) ||
            title.find('\0') != std::string_view::npos)
        {
            return std::nullopt;
        }
        const auto bytes = static_cast<int>(title.size());
        const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), bytes, nullptr, 0);
        if (length == 0)
        {
            return std::nullopt;
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), bytes, result.data(), length) != length)
        {
            return std::nullopt;
        }
        return result;
    }

    // 比较原生 caption 的完整 UTF-16 内容, 读取失败与空标题通过线程错误码区分.
    static bool ConfirmsTitle(GLFWwindow *window, std::wstring_view expected)
    {
        const auto hwnd = glfwGetWin32Window(window);
        if (!hwnd)
        {
            return false;
        }
        // 多读一个字符用于识别较长标题. GLFW 的缓存先于 Win32 调用更新, 不作生效凭据.
        std::wstring observed(expected.size() + 2, L'\0');
        SetLastError(ERROR_SUCCESS);
        const auto length = GetWindowTextW(hwnd, observed.data(), static_cast<int>(observed.size()));
        return (length != 0 || GetLastError() == ERROR_SUCCESS) &&
               std::wstring_view(observed.data(), static_cast<std::size_t>(length)) == expected;
    }

    // 交换区间由当前 drawable 解释. 同时确认 GLFW 身份, WGL context 和 DC 所属窗口.
    static bool HasCurrentDrawable(const DeviceHostState &state)
    {
        return glfwGetCurrentContext() == state.window && wglGetCurrentContext() == glfwGetWGLContext(state.window) &&
               WindowFromDC(wglGetCurrentDC()) == glfwGetWin32Window(state.window);
    }

    // 在已确认的 current context 上查扩展和入口, 每次在本轮使用范围内借用.
    static QuerySwapInterval FindSwapIntervalQuery()
    {
        if (!glfwExtensionSupported("WGL_EXT_swap_control"))
        {
            return nullptr;
        }
        return std::bit_cast<QuerySwapInterval>(glfwGetProcAddress("wglGetSwapIntervalEXT"));
    }

    // 字段拒绝通过实时快照供菜单读取, 已确认值留在各自的字段中.
    static HostError RejectSetting(DeviceHostState &state, std::optional<Core::BoundedText> &failure,
                                   std::string_view reason) noexcept
    {
        auto error = MakeError(&state, HostErrorReason::RequestRejected, HostStage::RequestApplication, reason);
        failure = error.context.description;
        return error;
    }

    // 从启动配置保留请求, 分别读回已有 caption 和区间. 无法确认的字段可在后续请求时重试.
    void InspectWindowSettings(DeviceHostState &state, const HostDesc &desc)
    {
        auto &settings = state.windowSettings;
        settings.window = state.info.identity;
        settings.requestedTitle = desc.title;
        const auto title = DecodeTitle(desc.title);
        if (title && ConfirmsTitle(state.window, *title))
        {
            settings.effectiveTitle = settings.requestedTitle;
        }
        else
        {
            settings.titleFailure.emplace("Initial window caption could not be confirmed");
        }
        settings.requestedSwapInterval = state.info.requestedSwapInterval;
        if (const auto query = HasCurrentDrawable(state) ? FindSwapIntervalQuery() : nullptr)
        {
            settings.effectiveSwapInterval = query();
        }
        else
        {
            settings.swapIntervalFailure.emplace("WGL swap interval confirmation is unavailable");
        }
    }

    // 先校验并记录本次请求, 捕获 GLFW 错误后再用实际 caption 确认有效值.
    std::optional<HostError> ApplyWindowTitle(DeviceHostState &state, std::string_view title)
    {
        auto &settings = state.windowSettings;
        const auto expected = title.size() <= MaxWindowTitleBytes ? DecodeTitle(title) : std::nullopt;
        if (!expected)
        {
            return RejectSetting(state, settings.titleFailure, "Title must be UTF-8 up to 255 bytes without NUL");
        }

        settings.requestedTitle = title;
        auto accepted = settings.requestedTitle;
        BeginNativeOperation(state);
        glfwSetWindowTitle(state.window, settings.requestedTitle.c_str());
        if (auto error = TakeNativeError(state, HostErrorReason::RequestRejected, HostStage::RequestApplication))
        {
            settings.titleFailure = error->context.description;
            return error;
        }
        if (!ConfirmsTitle(state.window, *expected))
        {
            return RejectSetting(state, settings.titleFailure, "Win32 did not confirm the requested window caption");
        }
        settings.effectiveTitle = std::move(accepted);
        settings.titleFailure.reset();
        return std::nullopt;
    }

    // 确认原生归属后提交 0 或 1. 窗口设置记录读回结果, 实际交换等待由 Present 单独观测.
    std::optional<HostError> ApplyWindowVSync(DeviceHostState &state, bool enabled)
    {
        if (!HasCurrentDrawable(state))
        {
            return MakeError(&state, HostErrorReason::ContextActivation, HostStage::RequestApplication,
                             "VSync requires the host's current WGL context and drawable");
        }

        auto &settings = state.windowSettings;
        const int interval = enabled ? 1 : 0;
        settings.requestedSwapInterval = interval;
        BeginNativeOperation(state);
        const auto query = FindSwapIntervalQuery();
        if (!query)
        {
            return RejectSetting(state, settings.swapIntervalFailure, "WGL swap interval confirmation is unavailable");
        }
        // GLFW 不传播 WGL setter 的返回值. 读取同一 drawable 的区间确认请求是否生效.
        glfwSwapInterval(interval);
        if (auto error = TakeNativeError(state, HostErrorReason::RequestRejected, HostStage::RequestApplication))
        {
            settings.swapIntervalFailure = error->context.description;
            return error;
        }
        if (query() != interval)
        {
            return RejectSetting(state, settings.swapIntervalFailure, "WGL did not confirm the requested swap interval");
        }
        settings.effectiveSwapInterval = interval;
        settings.swapIntervalFailure.reset();
        return std::nullopt;
    }
}
