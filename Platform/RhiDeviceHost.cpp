#include "DeviceHostInternal.hpp"

#include <limits>

#include <OpenGL.hpp>
#include <Validation.hpp>

namespace Physara::Platform::Detail
{
    void RhiMessageAdapter::Message(RHI::MessageSeverity severity, const char *message) noexcept
    {
        const std::string_view text = message != nullptr ? message : "RHI diagnostic without text";
        if (m_Capturing && severity == RHI::MessageSeverity::Error)
        {
            if (!m_First)
            {
                m_First.emplace(text);
            }
            else if (m_Additional != std::numeric_limits<std::uint64_t>::max())
            {
                ++m_Additional;
            }
        }
        // 先保存有界主错, 日志写入失败不能替换原始 RHI 结果.
        const auto level = severity == RHI::MessageSeverity::Error     ? Core::Severity::Error
                           : severity == RHI::MessageSeverity::Warning ? Core::Severity::Warning
                                                                       : Core::Severity::Info;
        static_cast<void>(m_Diagnostics.Write(level, text));
    }

    void RhiMessageAdapter::BeginCapture() noexcept
    {
        m_First.reset();
        m_Additional = 0;
        m_Capturing = true;
    }

    void RhiMessageAdapter::EndCapture() noexcept
    {
        m_Capturing = false;
    }

    static bool IsResourceContextCurrent(void *user) noexcept
    {
        const auto &state = *static_cast<DeviceHostState *>(user);
        return state.ownerThread == std::this_thread::get_id() && glfwGetCurrentContext() == state.window;
    }

    static HostError ResourceError(DeviceHostState &state, HostErrorReason reason, std::string_view fallback,
                                   std::uint32_t nativeCode = 0) noexcept
    {
        const auto &first = state.rhiMessages.First();
        auto error = MakeError(&state, reason, state.stage, first ? first->View() : fallback);
        error.additionalNativeErrors = state.rhiMessages.Additional();
        if (nativeCode != 0)
        {
            error.context.nativeCode = Core::NativeErrorCode{Core::NativeErrorDomain::OpenGL, nativeCode};
        }
        return error;
    }

    std::optional<HostError> InitializeResourceDevice(DeviceHostState &state)
    {
        state.stage = HostStage::RhiCreation;
        state.rhiMessages.BeginCapture();
        std::uint32_t nativeError = 0;
        const RHI::OpenGL::DeviceDesc desc{&state.rhiMessages, {&state, IsResourceContextCurrent}};
        state.rawDevice = RHI::OpenGL::CreateDevice(desc, &nativeError);
        if (state.rawDevice)
        {
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
            if (state.checks.failValidationComposition)
            {
                state.rhiMessages.Message(RHI::MessageSeverity::Error, "Injected validation composition failure");
            }
            else
#endif
            {
                state.validatedDevice = RHI::Validation::CreateValidationLayer(state.rawDevice.Get());
            }
        }
        state.rhiMessages.EndCapture();
        if (!state.validatedDevice)
        {
            return ResourceError(state, HostErrorReason::RhiCreation, "RHI device or validation creation failed",
                                 nativeError);
        }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected stop after RHI composition");
        }
#endif
        return std::nullopt;
    }

    bool HasExternalRhiOwners(const DeviceHostState &state) noexcept
    {
        // 调用方必须先收拢用户. 此处只验证已知根引用关系, 不作为并发停止协议.
        return (state.validatedDevice && state.validatedDevice->GetRefCount() != 1) ||
               (state.rawDevice && state.rawDevice->GetRefCount() != (state.validatedDevice ? 2ul : 1ul));
    }

    std::optional<HostError> ReleaseResourceDevice(DeviceHostState &state)
    {
        if (!state.rawDevice)
        {
            return std::nullopt;
        }
        state.stage = HostStage::RhiShutdown;
        state.rhiMessages.BeginCapture();
        std::optional<HostError> first;
        BeginNativeOperation(state);
        if (state.window != nullptr && glfwGetCurrentContext() != state.window)
        {
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
            if (!state.checks.failRhiCurrentRestore)
#endif
            {
                glfwMakeContextCurrent(state.window);
            }
        }
        first = TakeNativeError(state, HostErrorReason::RhiShutdown, state.stage);
        const bool current = !first && state.window != nullptr && glfwGetCurrentContext() == state.window;
        if (!current && !first)
        {
            first =
                MakeError(&state, HostErrorReason::RhiShutdown, state.stage,
                          "Owning context unavailable during final RHI shutdown; releasing context without GL cleanup");
        }
        if (current)
        {
            state.rawDevice->RunGarbageCollection();
        }
        state.validatedDevice.Reset();
        std::uint32_t nativeError = 0;
        const bool stopped = RHI::OpenGL::ShutdownDevice(
            state.rawDevice.Get(),
            current ? RHI::OpenGL::ShutdownMode::Normal : RHI::OpenGL::ShutdownMode::ContextUnavailable, &nativeError);
        if (!stopped)
        {
            MergeCleanupError(first,
                              ResourceError(state, HostErrorReason::RhiShutdown, "RHI cleanup failed", nativeError));
        }
        state.rawDevice.Reset();
        state.rhiMessages.EndCapture();
        return first;
    }

#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
    RHI::Device *DeviceHostChecks::BorrowRawDevice(DeviceHost &host) noexcept
    {
        const auto &state = *host.m_State;
        return state.ownerThread == std::this_thread::get_id() && state.ready ? state.rawDevice.Get() : nullptr;
    }
#endif
}
