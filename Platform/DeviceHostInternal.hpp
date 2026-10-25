#pragma once

#include "DeviceHost.hpp"

#include <chrono>
#include <source_location>
#include <thread>

#include <GLFW/glfw3.h>

#include <Core/Diagnostics.hpp>

#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
#include "DeviceHostChecks.hpp"
#endif

namespace Physara::Platform::Detail
{
    struct NativeCapture
    {
        std::optional<Core::ErrorContext> first;
        std::uint64_t additional = 0;
        std::source_location source = std::source_location::current();
    };

    struct DeviceHostState
    {
        explicit DeviceHostState(Core::Diagnostics &log) noexcept : diagnostics(log)
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
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        HostCheckConfig checks;
#endif
    };

    [[nodiscard]] HostError MakeError(const DeviceHostState *state, HostErrorReason reason, HostStage stage,
                                      std::string_view message,
                                      std::source_location source = std::source_location::current()) noexcept;
    void ReportError(Core::Diagnostics &diagnostics, const HostError &error) noexcept;
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
    [[nodiscard]] std::optional<HostError> ReleaseNativeState(DeviceHostState &state);

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
