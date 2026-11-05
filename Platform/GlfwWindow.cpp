#include "DeviceHostInternal.hpp"

// 独占进程 GLFW 生命周期与错误回调; 窗口/context 的取得和逆序释放都受同一宿主身份保护.

#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

namespace Physara::Platform::Detail
{
    struct GlfwProcessState
    {
        std::mutex mutex;
        DeviceHostState *active = nullptr;
        std::uint64_t lastGeneration = 0;
    };

    static GlfwProcessState s_Process;

    // GLFW 的错误描述只在回调期间有效. 锁内只保存首错/计数, 不再调用 GLFW 或日志消费者.
    static void CaptureGlfwError(int code, const char *description) noexcept
    {
        try
        {
            std::scoped_lock lock(s_Process.mutex);
            if (s_Process.active == nullptr)
            {
                return;
            }
            auto &capture = s_Process.active->capture;
            if (!capture.first)
            {
                capture.first = Physara::Core::ErrorContext{
                    Physara::Core::BoundedText(description != nullptr ? description : "GLFW error without text"),
                    Physara::Core::NativeErrorCode{Physara::Core::NativeErrorDomain::Glfw, code}, capture.source};
            }
            else if (capture.additional != std::numeric_limits<std::uint64_t>::max())
            {
                ++capture.additional;
            }
        }
        catch (...)
        {
            TerminateForContract("GLFW error capture synchronization failed");
        }
    }

    static bool ReleaseProcess(DeviceHostState &state)
    {
        std::scoped_lock lock(s_Process.mutex);
        if (s_Process.active != &state || !state.ownsProcess)
        {
            return false;
        }
        s_Process.active = nullptr;
        state.ownsProcess = false;
        return true;
    }

}

namespace Physara::Platform::Detail
{
    std::optional<HostError> ReserveProcess(DeviceHostState &state)
    {
        {
            std::scoped_lock lock(s_Process.mutex);
            if (s_Process.active != nullptr)
            {
                return MakeError(&state, HostErrorReason::AlreadyActive, HostStage::Reservation,
                                 "Only one initializing or active DeviceHost is supported");
            }
            if (!Core::AdvanceGeneration(s_Process.lastGeneration))
            {
                return MakeError(&state, HostErrorReason::IdentityExhausted, HostStage::Reservation,
                                 "Window identity generation exhausted");
            }
            state.info.identity = {0, s_Process.lastGeneration};
            state.ownsProcess = true;
            s_Process.active = &state;
        }
        return std::nullopt;
    }

    void BeginNativeOperation(DeviceHostState &state, std::source_location source)
    {
        std::scoped_lock lock(s_Process.mutex);
        state.capture.first.reset();
        state.capture.additional = 0;
        state.capture.source = source;
    }

    std::optional<HostError> TakeNativeError(DeviceHostState &state, HostErrorReason reason, HostStage stage)
    {
        std::scoped_lock lock(s_Process.mutex);
        if (!state.capture.first)
        {
            return std::nullopt;
        }
        auto error = MakeError(&state, reason, stage, "");
        error.context = *state.capture.first;
        error.additionalNativeErrors = state.capture.additional;
        return error;
    }

    std::optional<HostError> InitializeGlfw(DeviceHostState &state)
    {
        state.stage = HostStage::GlfwInitialization;
        BeginNativeOperation(state);
        state.previousCallback = glfwSetErrorCallback(CaptureGlfwError);
        state.callbackInstalled = true;
        state.initialized = glfwInit() == GLFW_TRUE;
        if (auto error = TakeNativeError(state, HostErrorReason::GlfwInitialization, state.stage))
        {
            return error;
        }
        if (!state.initialized)
        {
            return MakeError(&state, HostErrorReason::GlfwInitialization, state.stage,
                             "glfwInit returned false without an error description");
        }
        return std::nullopt;
    }

    std::optional<HostError> CreateGlfwWindow(DeviceHostState &state, const HostDesc &desc)
    {
        state.stage = HostStage::WindowCreation;
        const std::string title(desc.title);
        BeginNativeOperation(state);
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_DEBUG, desc.debugContext ? GLFW_TRUE : GLFW_FALSE);
        state.window = glfwCreateWindow(static_cast<int>(desc.clientWidthLogical),
                                        static_cast<int>(desc.clientHeightLogical), title.c_str(), nullptr, nullptr);
        if (auto error = TakeNativeError(state, HostErrorReason::WindowCreation, state.stage))
        {
            return error;
        }
        if (state.window == nullptr)
        {
            return MakeError(&state, HostErrorReason::WindowCreation, state.stage,
                             "glfwCreateWindow returned null without an error description");
        }
        glfwSetWindowUserPointer(state.window, &state);
        state.info.visible = glfwGetWindowAttrib(state.window, GLFW_VISIBLE) == GLFW_TRUE;
        if (auto error = TakeNativeError(state, HostErrorReason::WindowCreation, state.stage))
        {
            return error;
        }
        return std::nullopt;
    }

    std::optional<HostError> ActivateContext(DeviceHostState &state)
    {
        state.stage = HostStage::ContextActivation;
        BeginNativeOperation(state);
        glfwMakeContextCurrent(state.window);
        const bool current = glfwGetCurrentContext() == state.window;
        if (auto error = TakeNativeError(state, HostErrorReason::ContextActivation, state.stage))
        {
            return error;
        }
        if (!current)
        {
            return MakeError(&state, HostErrorReason::ContextActivation, state.stage,
                             "Created context did not become current on the owner thread");
        }
        return std::nullopt;
    }

    std::optional<HostError> ReleaseNativeState(DeviceHostState &state)
    {
        // 三个退出边界: 外部 owner 尚存时保持原状; drain Pending 时保留原生环境;
        // 可以最终释放后, 才按 RHI -> 输入/窗口/context -> GLFW -> 进程 claim 逆序退出.
        if (state.stopped)
        {
            return std::nullopt;
        }
        if (HasExternalRhiOwners(state))
        {
            return MakeError(&state, HostErrorReason::ResourceInUse, HostStage::RhiShutdown,
                             "Release all resources and extra Device owners before host shutdown");
        }
        state.ready = false;
        // 外部 owner 预检成功后才停止新工作. Pending 必须保留整套原生环境, 由调用方重试.
        state.inputAccepting = false;
        auto first = ReleaseResourceDevice(state);
        if (first && first->reason == HostErrorReason::RhiDrainPending)
        {
            return first;
        }
        state.stage = HostStage::Shutdown;
        if (state.window != nullptr)
        {
            MergeCleanupError(first, ReleaseWindowInput(state));
            BeginNativeOperation(state);
            glfwSetWindowUserPointer(state.window, nullptr);
            MergeCleanupError(first, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
            BeginNativeOperation(state);
            const auto current = glfwGetCurrentContext();
            if (current == state.window)
            {
                {
                    glfwMakeContextCurrent(nullptr);
                }
                if (glfwGetCurrentContext() == state.window)
                {
                    TerminateForContract("DeviceHost cannot safely detach its current context");
                }
            }
            MergeCleanupError(first, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
            BeginNativeOperation(state);
            glfwDestroyWindow(state.window);
            state.window = nullptr;
            MergeCleanupError(first, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
        }
        if (state.initialized)
        {
            BeginNativeOperation(state);
            glfwTerminate();
            state.initialized = false;
            MergeCleanupError(first, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
        }
        if (state.callbackInstalled)
        {
            glfwSetErrorCallback(state.previousCallback);
            state.callbackInstalled = false;
        }
        if (state.ownsProcess && !ReleaseProcess(state))
        {
            TerminateForContract("DeviceHost tried to release another owner's process claim");
        }
        state.stopped = true;
        return first;
    }
}
