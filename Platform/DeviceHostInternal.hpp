#pragma once

#include "DeviceHost.hpp"
#include "InputState.hpp"

#include <chrono>
#include <source_location>
#include <thread>

#include <GLFW/glfw3.h>

#include <Core/Diagnostics.hpp>
#include <RHI.hpp>

#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
#include "DeviceHostChecks.hpp"
#endif

namespace Physara::Platform::Detail
{
    class RhiMessageAdapter final : public RHI::MessageCallback
    {
    public:
        explicit RhiMessageAdapter(Core::Diagnostics &diagnostics) noexcept : m_Diagnostics(diagnostics)
        {
        }
        void Message(RHI::MessageSeverity severity, const char *message) noexcept override;
        void BeginCapture() noexcept;
        void EndCapture() noexcept;
        [[nodiscard]] const std::optional<Core::BoundedText> &First() const noexcept
        {
            return m_First;
        }
        [[nodiscard]] std::uint64_t Additional() const noexcept
        {
            return m_Additional;
        }

    private:
        Core::Diagnostics &m_Diagnostics;
        std::optional<Core::BoundedText> m_First;
        std::uint64_t m_Additional = 0;
        bool m_Capturing = false;
    };

    struct NativeCapture
    {
        std::optional<Core::ErrorContext> first;
        std::uint64_t additional = 0;
        std::source_location source = std::source_location::current();
    };

    struct DeviceHostState
    {
        explicit DeviceHostState(Core::Diagnostics &log) noexcept : diagnostics(log), rhiMessages(log)
        {
        }

        Core::Diagnostics &diagnostics;
        const std::thread::id ownerThread = std::this_thread::get_id();
        std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        HostInfo info;
        HostStage stage = HostStage::Reservation;
        GLFWwindow *window = nullptr;
        GLFWerrorfun previousCallback = nullptr;
        NativeCapture capture;
        bool ownsProcess = false;
        bool callbackInstalled = false;
        bool initialized = false;
        bool ready = false;
        bool stopped = false;
        InputState input;
        std::optional<HostError> inputFailure;
        bool inputCallbacksInstalled = false;
        bool inputAccepting = false;
        bool operationActive = false;
        // adapter 必须先于 Device 声明, 覆盖 raw/Validation 的全部 CPU 寿命.
        RhiMessageAdapter rhiMessages;
        RHI::DeviceHandle rawDevice;
        RHI::DeviceHandle validatedDevice;
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        HostCheckConfig checks;
#endif
    };

    [[nodiscard]] HostError MakeError(const DeviceHostState *state, HostErrorReason reason, HostStage stage,
                                      std::string_view message,
                                      std::source_location source = std::source_location::current()) noexcept;
    void ReportError(Core::Diagnostics &diagnostics, const HostError &error) noexcept;
    void AttachCleanupFailure(HostError &primary, const HostError &cleanup) noexcept;
    void MergeCleanupError(std::optional<HostError> &first, std::optional<HostError> next) noexcept;
    [[noreturn]] void TerminateForContract(std::string_view message) noexcept;

    [[nodiscard]] std::optional<HostError> ReserveProcess(DeviceHostState &state);
    void BeginNativeOperation(DeviceHostState &state, std::source_location source = std::source_location::current());
    [[nodiscard]] std::optional<HostError> TakeNativeError(DeviceHostState &state, HostErrorReason reason,
                                                           HostStage stage);
    [[nodiscard]] std::optional<HostError> InitializeGlfw(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> CreateGlfwWindow(DeviceHostState &state, const HostDesc &desc);
    [[nodiscard]] std::optional<HostError> ActivateContext(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> LoadGlEntries(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> InspectEnvironment(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> RequestSwapInterval(DeviceHostState &state, bool vsync);
    [[nodiscard]] std::optional<HostError> InitializeResourceDevice(DeviceHostState &state);
    [[nodiscard]] bool HasExternalRhiOwners(const DeviceHostState &state) noexcept;
    [[nodiscard]] std::optional<HostError> ReleaseResourceDevice(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> ReleaseNativeState(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> InitializeWindowInput(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> ReleaseWindowInput(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> ReleaseRelativeCursor(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> PollNativeInput(DeviceHostState &state);
    [[nodiscard]] std::optional<HostError> WaitNativeInput(DeviceHostState &state, double timeoutSeconds);
    [[nodiscard]] std::optional<HostError> ApplyNativeRequests(DeviceHostState &state,
                                                               std::span<const HostRequest> requests);
    [[nodiscard]] std::optional<HostError> TakeInputFault(DeviceHostState &state);

    struct DeviceHostFactory
    {
        [[nodiscard]] static DeviceHost::CreateResult Create(const HostDesc &desc, Core::Diagnostics &diagnostics
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
                                                             ,
                                                             const HostCheckConfig *checks = nullptr
#endif
                                                             ) noexcept;
    };
}
