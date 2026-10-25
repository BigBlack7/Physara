#include "DeviceHostInternal.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{
    using namespace Physara::Platform;
    using namespace Physara::Platform::Detail;

    struct GlfwProcessState
    {
        std::mutex mutex;
        DeviceHostState *active = nullptr;
        std::uint64_t lastGeneration = 0;
    };

    GlfwProcessState s_Process;

    void CaptureGlfwError(int code, const char *description) noexcept
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

    bool ReleaseProcess(DeviceHostState &state)
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

    void MergeCleanupError(std::optional<HostError> &first, std::optional<HostError> next)
    {
        if (!next)
        {
            return;
        }
        if (!first)
        {
            first = std::move(next);
            return;
        }
        constexpr auto MaxCount = std::numeric_limits<std::uint64_t>::max();
        const auto additional = next->additionalNativeErrors == MaxCount ? MaxCount : next->additionalNativeErrors + 1;
        first->additionalNativeErrors += std::min(additional, MaxCount - first->additionalNativeErrors);
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == HostStage::Reservation)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage, "Injected stop after reservation");
        }
#endif
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage, "Injected stop after GLFW init");
        }
#endif
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.impossibleWindowVersion)
        {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 99);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        }
#endif
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected stop after window creation");
        }
#endif
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected stop after context activation");
        }
#endif
        return std::nullopt;
    }

    std::optional<HostError> ReleaseNativeState(DeviceHostState &state)
    {
        if (state.stopped)
        {
            return std::nullopt;
        }
        state.ready = false;
        state.stage = HostStage::Shutdown;
        std::optional<HostError> first;
        if (state.window != nullptr)
        {
            BeginNativeOperation(state);
            glfwSetWindowUserPointer(state.window, nullptr);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
            if (state.checks.cleanupError)
            {
                CaptureGlfwError(GLFW_PLATFORM_ERROR, "Injected cleanup error");
            }
#endif
            MergeCleanupError(first, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
            BeginNativeOperation(state);
            const auto current = glfwGetCurrentContext();
            if (current == state.window)
            {
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
                if (!state.checks.detachFailure)
#endif
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

#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
    std::uint64_t DeviceHostChecks::SetLastGenerationWhenIdle(std::uint64_t value)
    {
        std::scoped_lock lock(s_Process.mutex);
        if (s_Process.active != nullptr)
        {
            throw std::logic_error("Generation checks require an idle process");
        }
        const auto previous = s_Process.lastGeneration;
        s_Process.lastGeneration = value;
        return previous;
    }

    bool DeviceHostChecks::RejectForeignRelease(Core::Diagnostics &diagnostics)
    {
        DeviceHostState foreign(diagnostics);
        return !ReleaseProcess(foreign);
    }

    HostError DeviceHostChecks::CaptureTemporaryDescription(DeviceHost &host, std::string_view text)
    {
        auto &state = *host.m_State;
        BeginNativeOperation(state);
        {
            const std::string temporary(text);
            CaptureGlfwError(GLFW_PLATFORM_ERROR, temporary.c_str());
        }
        return *TakeNativeError(state, HostErrorReason::EnvironmentQuery, HostStage::EnvironmentQuery);
    }
#endif
}
