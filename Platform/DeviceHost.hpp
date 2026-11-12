#pragma once

/// @file
/// @brief 发布窗口和输入事实, 借出 RHI Device, 并在 owner 线程推进索引呈现与关闭.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include "Input.hpp"
#include "Presentation.hpp"

#include <Core/Error.hpp>
#include <Core/Identity.hpp>

namespace Physara::Core
{
    class Diagnostics;
}

namespace Physara::RHI
{
    class Device;
}

namespace Physara::Platform
{
    /// @brief 一个拟录制包的上传准入结果. Pending 可随已有包退休恢复.
    enum class UploadAdmission : std::uint8_t
    {
        Ready,
        Pending,
        ExceedsLimit,
        InvalidState
    };

    namespace Detail
    {
        struct DeviceHostState;
        struct DeviceHostFactory;
        struct GlfwAccess;
    }

    /// @brief 创建一个窗口及 OpenGL context 的配置, 尺寸使用窗口逻辑单位.
    struct HostDesc
    {
        std::uint32_t clientWidthLogical = 1280;
        std::uint32_t clientHeightLogical = 720;
        // 仅在 Create 调用期间借用, GLFW 创建时复制标题.
        std::string_view title = "Physara";
        bool visible = true;
        bool vsync = true;
        bool debugContext = false;
        std::size_t presentationSlotCount = 1; ///< Runtime 传入其固定帧槽数, 须位于 [1, MaxPresentationSlots].
    };

    enum class HostPlatform : std::uint8_t
    {
        Unknown,
        Win32
    };

    /// @brief 创建时查询的默认后缓冲颜色编码, 供末端复制选择转换方式.
    enum class FramebufferEncoding : std::uint8_t
    {
        Unverified,
        Linear,
        SRGB
    };

    /// @brief 创建成功时的自持环境快照, 可在宿主关闭后保留.
    /// @note 当前窗口尺寸和缩放以 PollEvents 的 WindowState 为准, 此值不充当实时能力缓存.
    struct HostInfo
    {
        // 创建时的自持环境快照, 不是当前窗口尺寸或实时 caps 的第二份权威状态.
        // 当前逻辑尺寸/像素尺寸/缩放由 PollEvents 返回的 WindowState 表达.
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
        // 记录请求的间隔, 不表示驱动实际执行的等待或测得的呈现耗时.
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
        Shutdown,
        InputSetup,
        EventPolling,
        EventWait,
        RequestApplication,
        RhiCreation,
        RhiShutdown,
        Presentation
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
        InternalFailure,
        Stopped,
        ReentrantCall,
        InputOverflow,
        InputInvalidData,
        CounterExhausted,
        EventProcessing,
        RequestApplication,
        RequestRejected,
        RhiCreation,
        RhiShutdown,
        ResourceInUse,
        RhiDrainPending,
        PresentationFailed
    };

    /// @brief 一次宿主操作的首错及附加错误计数, 保留创建代与清理失败上下文.
    struct HostError
    {
        HostErrorReason reason = HostErrorReason::InternalFailure;
        HostStage stage = HostStage::Validation;
        std::optional<WindowId> attempt; ///< 尚未取得进程创建代时为空.
        Core::ErrorContext context;
        std::uint64_t additionalNativeErrors = 0;
        std::optional<Core::ErrorContext> cleanupError;
        std::uint64_t additionalCleanupErrors = 0;
        double elapsedSeconds = 0.0;
        std::optional<std::size_t> requestIndex; ///< 请求应用失败时定位批内首个失败项.
    };

    /// @brief 无分配地格式化自持错误描述, 不执行原生操作, 返回后可跨线程读取.
    [[nodiscard]] Core::BoundedText DescribeHostError(const HostError &error) noexcept;

    /// @brief 从取样时刻起观察同一宿主 Device 的 Error/Fatal, 供同步命令决定是否继续.
    /// @note 借用宿主回调中的计数. 在 owner 线程使用并在 Shutdown 前结束.
    class RhiErrorScope final
    {
    public:
        /// @brief 取样有效且此后未观察到 Error/Fatal 时为 true. 计数耗尽视为失败.
        /// @note 结果独立于日志写入, 提交是否接受及 GPU 是否完成由调用者另行确认.
        [[nodiscard]] bool IsClear() const noexcept;

    private:
        friend class DeviceHost;
        const std::atomic<std::uint64_t> *m_ErrorCount = nullptr;
        std::uint64_t m_InitialCount = 0;
    };

    /// @brief 独占进程 GLFW 生命周期, 拥有窗口, context, 输入与 raw/Validation Device.
    /// 按应用提供的索引保有窗口目标和完成观察, 每次只推进一个活动取得.
    /// @par 线程与寿命
    /// 首次 Create 从进程 main 线程调用; 活跃操作限创建线程且不可重入.
    /// diagnostics 必须活到本 owner 完成关闭, 原生借用不得跨成功关闭保留.
    class DeviceHost final
    {
    public:
        using CreateResult = std::variant<std::unique_ptr<DeviceHost>, HostError>;
        using PollResult = std::variant<HostFrameInput, HostError>;

        /// @brief 校验配置并按阶段装配宿主, 任一初始化失败都尝试逆序回滚.
        /// @return Ready 的独占宿主或携带首错及清理上下文的 HostError.
        [[nodiscard]] static CreateResult Create(const HostDesc &desc, Core::Diagnostics &diagnostics) noexcept;
        ~DeviceHost();
        DeviceHost(const DeviceHost &) = delete;
        DeviceHost &operator=(const DeviceHost &) = delete;
        DeviceHost(DeviceHost &&) = delete;
        DeviceHost &operator=(DeviceHost &&) = delete;

        /// @brief 收拢 RHI 后按逆序释放窗口/context/GLFW, owner 线程重复关闭成功.
        /// @pre 先停止并 join 资源使用者, 释放全部资源及额外 Device owner.
        /// @return 宿主先释放自有呈现句柄. ResourceInUse 保留设备和窗口, 释放外部引用后可重试.
        /// RhiDrainPending 停止新工作并保留 context/owner, 须重试至停止后再析构.
        /// 最终删除开始后即使带错也完成停止.
        /// @note 无硬实时保证; 错误线程析构或析构前未收拢 owner 是致命合同违例.
        [[nodiscard]] std::optional<HostError> Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;
        /// @brief 取得自持环境值, 拷贝可跨线程或关闭保留; 活跃 host 本身不并发访问.
        [[nodiscard]] HostInfo GetInfo() const noexcept;
        /// @brief 仅在 owner/Ready 状态借出 Validation Device, 其他情况返回空指针.
        /// @note 不增加引用, 借用止于成功 Shutdown; 构造 owning handle 取得的额外引用须先释放.
        [[nodiscard]] RHI::Device *GetDevice() const noexcept;
        /// @brief 在 owner/Ready 状态取样 RHI 错误计数. 多个区间各自持有起点, 可以重叠.
        /// @return 无法取样时返回 IsClear 为 false 的值.
        [[nodiscard]] RhiErrorScope CaptureRhiErrors() const noexcept;

        /// @brief 预检新 Graphics 包的纹理和缓冲写入. 尺寸为单次写入的紧密字节数, 0 表示省略.
        /// @pre 宿主就绪且 owner 持有 current context. 无开放录制, chunkSize 取自拟使用的列表.
        /// 提供的序列与实际录制一致, 纹理写入后录制缓冲写入.
        /// @return Ready 表示当前余量容得下该包, Pending 随设备退休可重试.
        /// @note 只读取容量. 准入后须在同一 owner 上先录制该包.
        [[nodiscard]] UploadAdmission CheckUploadCapacity(std::size_t chunkSize,
                                                           std::span<const std::size_t> textureWrites,
                                                           std::span<const std::size_t> bufferWrites) const noexcept;

        /// @brief 一次取得返回拥有型帧, 正常暂不可用状态或宿主错误.
        using PresentationAcquire = std::variant<PresentationFrame, PresentationStatus, HostError>;
        /// @brief 按索引取得最近发布的窗口像素目标, 返回帧时可复用该槽的帧内存储.
        /// @pre 宿主就绪, 由 owner 线程调用. 可用的取得经 Present 或 CancelPresentation 结束.
        /// @return 槽未完成时为 Retry, 窗口暂停时为 Suspended. 返回错误时无新取得.
        [[nodiscard]] PresentationAcquire BeginPresentation(std::size_t slotIndex) noexcept;
        /// @brief 关联本次取得和闭合 Graphics 列表. 颜色有效且尺寸仍匹配时发出末端复制.
        /// @param frameSucceeded 表示本次录制已完整初始化颜色, 并且录制与 Close 区间均成功.
        /// @pre commands 来自此宿主的验证设备. 准备成功后由 SubmitPresentation 消费同一次录制.
        /// @return 空值表示可进入提交, 失败帧或尺寸过期时提交仅保护已发前缀.
        /// RequestRejected 保留当前取得. 末端原生失败进入宿主关闭收拢.
        [[nodiscard]] std::optional<HostError> PrepareSubmission(PresentationToken token, RHI::CommandList &commands,
                                                                bool frameSucceeded) noexcept;
        /// @brief 委托验证设备提交已关联的列表, 将接受票据绑定到槽的 Event 观察.
        /// @return 设备接受的票据与颜色资格. 即使后续 Event 设置失败, 非零票据仍会返回.
        /// @param error 写入本次操作错误. 为空时继续调用 Present 消费取得.
        [[nodiscard]] PresentationSubmission SubmitPresentation(PresentationToken token,
                                                                std::optional<HostError> &error) noexcept;
        /// @brief 结束本次已提交取得, 颜色和窗口仍有效时尝试一次交换.
        /// @return 已接受票据始终关联该次提交. error 报告交换故障时仍可读实际尝试及等待区间.
        [[nodiscard]] PresentationResult Present(PresentationToken token, std::optional<HostError> &error) noexcept;
        /// @brief 退还尚未关联列表的取得, 供准备阶段取消.
        /// @note 若已经发出命令, 调用者须在原列表上完成提交或沿设备退出合同收拢前缀.
        [[nodiscard]] std::optional<HostError> CancelPresentation(PresentationToken token) noexcept;

        /// @brief 消费原生事件并发布一批有序输入; 接受的 poll 开始即结束上批 events 借用.
        /// @return 完整输入批次或 HostError, 输入故障不发布成功的部分批次.
        [[nodiscard]] PollResult PollEvents() noexcept;
        /// @brief 等待有限正秒并累积回调, 不发布输入, 不缩短已有 events 借用.
        [[nodiscard]] std::optional<HostError> WaitForEvents(double timeoutSeconds) noexcept;
        /// @brief 顺序应用本次借用的请求, 不保存队列; 原生失败允许此前缀已经生效.
        /// @return 成功为空; 失败的 HostError 通过 requestIndex 定位首个失败项.
        [[nodiscard]] std::optional<HostError> ApplyDeferredRequests(std::span<const HostRequest> requests) noexcept;

    private:
        explicit DeviceHost(std::unique_ptr<Detail::DeviceHostState> state) noexcept;
        std::unique_ptr<Detail::DeviceHostState> m_State;
        friend struct Detail::DeviceHostFactory;
        friend struct Detail::GlfwAccess;
    };
}
