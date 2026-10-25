#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>

namespace Physara::Core
{
    class Diagnostics;
}

namespace Physara::Platform
{
    namespace Detail
    {
        struct DeviceHostState;
        struct DeviceHostFactory;
        struct DeviceHostChecks;
        struct GlfwAccess;
    }

    struct WindowIdentityTag;
    using WindowId = Core::Identity<WindowIdentityTag>;

    struct HostDesc
    {
        std::uint32_t clientWidthLogical = 1280;
        std::uint32_t clientHeightLogical = 720;
        // 仅在 Create 调用期间借用, GLFW 创建时复制标题.
        std::string_view title = "Physara";
        bool visible = true;
        bool vsync = true;
        bool debugContext = false;
    };

    enum class HostPlatform : std::uint8_t
    {
        Unknown,
        Win32
    };

    enum class FramebufferEncoding : std::uint8_t
    {
        Unverified,
        Linear,
        SRGB
    };

    struct HostInfo
    {
        WindowId identity;
        HostPlatform platform = HostPlatform::Unknown;
        std::uint32_t glfwMajor = 0;
        std::uint32_t glfwMinor = 0;
        std::uint32_t glfwRevision = 0;
        std::uint32_t glMajor = 0;
        std::uint32_t glMinor = 0;
        std::uint32_t contextFlags = 0;
        bool coreProfile = false;
        bool doubleBuffered = false;
        bool visible = false;
        bool debugContext = false;
        FramebufferEncoding defaultFramebufferEncoding = FramebufferEncoding::Unverified;
        Core::BoundedText vendor;
        Core::BoundedText renderer;
        Core::BoundedText version;
        Core::BoundedText shadingLanguageVersion;
        std::uint32_t maxTextureSizePixels = 0;
        std::uint32_t maxRenderbufferSizePixels = 0;
        std::uint32_t maxUniformBufferBindings = 0;
        std::uint32_t maxStorageBufferBindings = 0;
        std::uint32_t maxCombinedTextureUnits = 0;
        std::uint32_t uniformBufferOffsetAlignmentBytes = 0;
        // 请求不等于驱动实际等待行为; 本轮不测量 PresentWait.
        std::int32_t requestedSwapInterval = 0;
        double creationSeconds = 0.0;
    };

    enum class HostStage : std::uint8_t
    {
        Validation,
        Reservation,
        GlfwInitialization,
        WindowCreation,
        ContextActivation,
        EntryLoading,
        EnvironmentQuery,
        SwapInterval,
        Ready,
        Shutdown
    };

    enum class HostErrorReason : std::uint8_t
    {
        InvalidConfiguration,
        AlreadyActive,
        WrongThread,
        IdentityExhausted,
        OutOfMemory,
        GlfwInitialization,
        WindowCreation,
        ContextActivation,
        EntryLoading,
        UnsupportedContext,
        EnvironmentQuery,
        SwapInterval,
        Cleanup,
        InternalFailure
    };

    struct HostError
    {
        HostErrorReason reason = HostErrorReason::InternalFailure;
        HostStage stage = HostStage::Validation;
        std::optional<WindowId> attempt;
        Core::ErrorContext context;
        std::uint64_t additionalNativeErrors = 0;
        std::optional<Core::ErrorContext> cleanupError;
        std::uint64_t additionalCleanupErrors = 0;
        double elapsedSeconds = 0.0;
    };

    // 首次 Create 必须从进程 main 线程调用, 且本类独占进程 GLFW 生命周期.
    // diagnostics 必须活到本 owner 完成关闭; 所有活跃宿主操作限创建线程.
    class DeviceHost final
    {
      public:
        using CreateResult = std::variant<std::unique_ptr<DeviceHost>, HostError>;

        [[nodiscard]] static CreateResult Create(const HostDesc &desc, Core::Diagnostics &diagnostics) noexcept;
        ~DeviceHost();
        DeviceHost(const DeviceHost &) = delete;
        DeviceHost &operator=(const DeviceHost &) = delete;
        DeviceHost(DeviceHost &&) = delete;
        DeviceHost &operator=(DeviceHost &&) = delete;

        // 错误线程不执行原生操作; owner 线程重复关闭成功. 错误线程析构是致命合同违例.
        [[nodiscard]] std::optional<HostError> Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;
        // 自持值拷贝可跨线程/关闭保留; 活跃 host 本身不并发访问.
        [[nodiscard]] HostInfo GetInfo() const noexcept;

      private:
        explicit DeviceHost(std::unique_ptr<Detail::DeviceHostState> state) noexcept;
        std::unique_ptr<Detail::DeviceHostState> m_State;
        friend struct Detail::DeviceHostFactory;
        friend struct Detail::DeviceHostChecks;
        friend struct Detail::GlfwAccess;
    };
}
