#include "DeviceHostInternal.hpp"

/// @file
/// @brief 在 GLFW context 与 RHI 互操作边界装配 raw/Validation 根, 捕获领域诊断并收拢上传与删除.

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

    // 供原生后端在每次受限操作前确认 owner 线程与实际 current context.
    static bool IsResourceContextCurrent(void *user) noexcept
    {
        const auto &state = *static_cast<DeviceHostState *>(user);
        return state.ownerThread == std::this_thread::get_id() && glfwGetCurrentContext() == state.window;
    }

    // 首个 RHI 回调优先于兜底正文, 原生错误码独立保存, 不依赖日志写入成功.
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
        return std::nullopt;
    }

    bool HasExternalRhiOwners(const DeviceHostState &state) noexcept
    {
        // 调用方必须先收拢用户. 此处只验证已知根引用关系, 不作为并发停止协议.
        return (state.validatedDevice && state.validatedDevice->GetRefCount() != 1) ||
               (state.rawDevice &&
                RHI::OpenGL::CheckShutdownPreconditions(state.rawDevice.Get(), state.validatedDevice ? 2ul : 1ul) !=
                    RHI::OpenGL::ShutdownPrecondition::Ready);
    }

    std::optional<HostError> ReleaseResourceDevice(DeviceHostState &state)
    {
        // 调用者已完成外部引用预检. 先恢复 owner context, 再 drain; 不可用时由 Host 最终销毁 context.
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
        std::uint32_t nativeError = 0;
        const auto result = RHI::OpenGL::DrainDeviceWork(
            state.rawDevice.Get(),
            current ? RHI::OpenGL::ShutdownMode::Normal : RHI::OpenGL::ShutdownMode::ContextUnavailable, &nativeError);
        if (result != RHI::OpenGL::DrainResult::Ready)
        {
            // 此时两份根引用、adapter 和窗口仍须一起存活, 不能让重试面对已销毁的 callback/context.
            state.rhiMessages.EndCapture();
            return MakeError(&state, HostErrorReason::RhiDrainPending, state.stage,
                             "RHI drain incomplete; retain host and retry Shutdown before destruction");
        }
        if (nativeError != 0 || state.rhiMessages.First())
        {
            MergeCleanupError(first,
                              ResourceError(state, HostErrorReason::RhiShutdown, "RHI cleanup failed", nativeError));
        }
        state.validatedDevice.Reset();
        state.rawDevice.Reset();
        state.rhiMessages.EndCapture();
        return first;
    }
}
