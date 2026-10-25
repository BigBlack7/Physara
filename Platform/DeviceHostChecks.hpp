#pragma once

#if !defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
#error This private header is available only to the temporary native checks.
#endif

#include "DeviceHost.hpp"

namespace Physara::Platform::Detail
{
    enum class MissingEntry : std::uint8_t
    {
        None,
        VersionString,
        BooleanQuery
    };

    struct HostCheckConfig
    {
        std::optional<HostStage> stopAfter;
        MissingEntry missingEntry = MissingEntry::None;
        bool impossibleWindowVersion = false;
        bool lowerObservedVersion = false;
        bool compatibilityObservedProfile = false;
        bool linearObservedEncoding = false;
        bool singleBufferedObserved = false;
        bool invalidObservedLimit = false;
        bool unknownObservedEncoding = false;
        bool nonDefaultObservedFramebuffer = false;
        bool queryError = false;
        bool swapIntervalError = false;
        bool cleanupError = false;
        bool detachFailure = false;
        std::optional<int> failInputSetupStep;
        std::optional<HostStage> nativeInputError;
        std::optional<HostStage> nativeObservationError;
        std::optional<std::size_t> failRequestIndex;
        bool failCursorRelease = false;
        bool loseFocusOnCapture = false;
    };

    struct DeviceHostChecks
    {
        // 仅临时进程检查在首次工厂调用前配置, 不进入生产编译/启动参数.
        static void SetNextCreateConfig(const HostCheckConfig &config);
        static void SetOperationObserver(void (*observer)(HostStage, Core::Diagnostics &));
        [[nodiscard]] static DeviceHost::CreateResult Create(const HostDesc &desc, Core::Diagnostics &diagnostics,
                                                             const HostCheckConfig &config) noexcept;
        [[nodiscard]] static std::uint64_t SetLastGenerationWhenIdle(std::uint64_t value);
        [[nodiscard]] static bool RejectForeignRelease(Core::Diagnostics &diagnostics);
        [[nodiscard]] static HostError CaptureTemporaryDescription(DeviceHost &host, std::string_view text);
        static void InjectInput(DeviceHost &host, const InputEvent &event);
        static void SetInputCounters(DeviceHost &host, std::uint64_t sequence, std::uint64_t revision);
        [[nodiscard]] static InputSnapshot InspectInput(DeviceHost &host);
        [[nodiscard]] static int InspectCursorMode(DeviceHost &host);
        [[nodiscard]] static bool InspectCloseFlag(DeviceHost &host);
    };
}
