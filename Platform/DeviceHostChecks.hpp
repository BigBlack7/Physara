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
    };

    struct DeviceHostChecks
    {
        [[nodiscard]] static DeviceHost::CreateResult Create(const HostDesc &desc, Core::Diagnostics &diagnostics,
                                                             const HostCheckConfig &config) noexcept;
        [[nodiscard]] static std::uint64_t SetLastGenerationWhenIdle(std::uint64_t value);
        [[nodiscard]] static bool RejectForeignRelease(Core::Diagnostics &diagnostics);
        [[nodiscard]] static HostError CaptureTemporaryDescription(DeviceHost &host, std::string_view text);
    };
}
