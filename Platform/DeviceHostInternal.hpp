#pragma once

/// @file
/// @brief 宿主私有的共享状态及阶段入口. 原生窗口, 设置, 输入和 RHI 根引用归同一 State.

#include "DeviceHost.hpp"
#include "InputState.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <source_location>
#include <thread>

#include <GLFW/glfw3.h>

#include <Core/Diagnostics.hpp>
#include <RHI.hpp>

namespace Physara::Platform::Detail
{
    /// @brief 先记录 RHI 失败序号, 再写入诊断. 创建/关闭区间额外保存首错和附加错误数.
    /// @note 借用 Diagnostics, 同宿主一起存活. 捕获结果不用于推断 device loss.
    class RhiMessageAdapter final : public RHI::MessageCallback
    {
    public:
        explicit RhiMessageAdapter(Core::Diagnostics &diagnostics) noexcept : m_Diagnostics(diagnostics)
        {
        }
        /// @brief 记录 Error/Fatal 后转发诊断. 首错捕获区间还会复制首条失败正文.
        void Message(RHI::MessageSeverity severity, const char *message) noexcept override;
        /// @brief 开始本次创建或关闭的首错捕获. EndCapture 后保留结果供宿主读取.
        void BeginCapture() noexcept;
        void EndCapture() noexcept;
        /// @brief 借用贯穿宿主寿命的失败计数, 供同步命令区间取样.
        [[nodiscard]] const std::atomic<std::uint64_t> &ErrorCount() const noexcept
        {
            return m_ErrorCount;
        }
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
        std::atomic<std::uint64_t> m_ErrorCount{0}; ///< Error/Fatal 的累计计数, 饱和后保持最大值.
        // 创建和关闭阶段独立保留首错正文与附加错误数.
        std::optional<Core::BoundedText> m_First;
        std::uint64_t m_Additional = 0;
        bool m_Capturing = false;
    };

    /// @brief 一次原生操作的首错和饱和计数, 全局 GLFW 错误回调在进程锁内写入.
    struct NativeCapture
    {
        std::optional<Core::ErrorContext> first;
        std::uint64_t additional = 0;
        std::source_location source = std::source_location::current();
    };

    /// @brief 取得的 CPU 阶段. Idle 仍可保有等待 Event 的上一笔提交.
    enum class PresentationPhase : std::uint8_t
    {
        Idle,
        Acquired,
        Prepared,
        Submitted
    };

    /// @brief 固定索引的窗口目标和完成观察. lastSubmission 非零时保留最近一次接受的设备票据.
    /// @note 同槽再取得时用 Event 确认上次完成, 之后才复用或替换目标.
    struct PresentationSlot
    {
        PresentTarget target;
        RHI::EventQueryHandle event;
        std::uint64_t targetGeneration = 0;
        std::uint64_t lastSubmission = 0;
    };

    /// @brief 同一窗口可有多个在途槽, owner 线程一次只推进一个 CPU 取得.
    /// @note Prepared 阶段强持验证列表. 成功设置 Event 后释放, 失败时由关闭路径解除.
    /// 各槽的 GPU 依赖仍由已发的物理包持有.
    struct PresentationState
    {
        std::array<PresentationSlot, MaxPresentationSlots> slots;
        std::size_t slotCount = 1;
        RHI::CommandListHandle commands;
        PresentationToken token;
        PresentationPhase phase = PresentationPhase::Idle;
        std::uint64_t acquisition = 0;
        bool colorValid = false;
        bool stopped = false;
    };

    /// @brief 按取得阶段记录资源的宿主 owner, 支持初始化中途的逆序回滚.
    /// @invariant stopped 仅在最终释放后置位; drain Pending 时保留窗口, adapter 和 Device 根引用.
    struct DeviceHostState
    {
        explicit DeviceHostState(Core::Diagnostics &log) noexcept : diagnostics(log), rhiMessages(log)
        {
        }

        Core::Diagnostics &diagnostics;
        const std::thread::id ownerThread = std::this_thread::get_id();
        std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        HostInfo info;
        /// @brief 启动读回建立首份设置快照, 此后由 owner 的窗口请求入口按字段更新.
        WindowSettings windowSettings;
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
        PresentationState presentation;
    };

    /// @brief 构造自持宿主错误, state 可空; 有状态时附上有效身份及已耗秒数.
    [[nodiscard]] HostError MakeError(const DeviceHostState *state, HostErrorReason reason, HostStage stage,
                                      std::string_view message,
                                      std::source_location source = std::source_location::current()) noexcept;
    /// @brief 以原报告位置记录宿主错误, 诊断失败不改变领域结果.
    void ReportError(Core::Diagnostics &diagnostics, const HostError &error) noexcept;
    /// @brief 保留主错并附加首个清理失败, 后续清理错误只累计饱和计数.
    void AttachCleanupFailure(HostError &primary, const HostError &cleanup) noexcept;
    /// @brief 合并清理阶段错误, 保留首次错误的正文与上下文.
    void MergeCleanupError(std::optional<HostError> &first, std::optional<HostError> next) noexcept;
    /// @brief 无法安全继续生命周期时输出有界原因并终止进程, 不伪造可恢复错误.
    [[noreturn]] void TerminateForContract(std::string_view message) noexcept;

    /// @brief 取得进程唯一宿主 claim 并分配不回绕的窗口代, 不初始化 GLFW.
    [[nodiscard]] std::optional<HostError> ReserveProcess(DeviceHostState &state);
    /// @brief 在进程锁内开始新的 GLFW 错误捕获区间, 不消费窗口输入事件.
    void BeginNativeOperation(DeviceHostState &state, std::source_location source = std::source_location::current());
    /// @brief 将当前捕获复制为宿主错误, 捕获内容保留至下一次 BeginNativeOperation.
    [[nodiscard]] std::optional<HostError> TakeNativeError(DeviceHostState &state, HostErrorReason reason,
                                                           HostStage stage);
    /// @brief 安装可恢复的进程错误回调并初始化 GLFW, 记录已取得阶段供回滚.
    [[nodiscard]] std::optional<HostError> InitializeGlfw(DeviceHostState &state);
    /// @brief 按固定 GL 4.6 Core profile 创建窗口, 将回调 user pointer 绑定到 state.
    [[nodiscard]] std::optional<HostError> CreateGlfwWindow(DeviceHostState &state, const HostDesc &desc);
    /// @brief 在 owner 线程激活窗口 context 并核对实际 current 状态.
    [[nodiscard]] std::optional<HostError> ActivateContext(DeviceHostState &state);
    /// @brief 通过 current context 加载固定 GL 入口, 拒绝缺少必要查询入口的环境.
    [[nodiscard]] std::optional<HostError> LoadGlEntries(DeviceHostState &state);
    /// @brief 查询并验证实际 profile/默认 framebuffer/限制, 写入自持启动快照.
    [[nodiscard]] std::optional<HostError> InspectEnvironment(DeviceHostState &state);
    /// @brief 请求交换间隔并记录请求值, 不测量或保证驱动实际等待行为.
    [[nodiscard]] std::optional<HostError> RequestSwapInterval(DeviceHostState &state, bool vsync);
    /// @brief 在原生窗口和 context 就绪后读取首份实时设置. 某字段无法确认时记入该字段原因.
    void InspectWindowSettings(DeviceHostState &state, const HostDesc &desc);
    /// @brief 校验 UTF-8 并应用 caption, 读回与请求一致后确认. 调用者已检查线程与窗口身份.
    [[nodiscard]] std::optional<HostError> ApplyWindowTitle(DeviceHostState &state, std::string_view title);
    /// @brief 在确认 context 和 drawable 归属后应用交换区间, 以原生查询确认实际值.
    [[nodiscard]] std::optional<HostError> ApplyWindowVSync(DeviceHostState &state, bool enabled);
    /// @brief 在原生 context 上建立 raw Device, 再建立持有它的 Validation 包装.
    [[nodiscard]] std::optional<HostError> InitializeResourceDevice(DeviceHostState &state);
    /// @brief 预检已知根引用之外的 owner, 调用前使用者须已停止, 不承担并发停止协议.
    [[nodiscard]] bool HasExternalRhiOwners(const DeviceHostState &state) noexcept;
    /// @brief 收拢设备工作后释放两层根引用; Pending 时保留全部原生环境供关闭重试.
    [[nodiscard]] std::optional<HostError> ReleaseResourceDevice(DeviceHostState &state);
    /// @brief 执行分阶段幂等退出, 外部 owner 拒绝或 drain Pending 时不销毁原生环境.
    [[nodiscard]] std::optional<HostError> ReleaseNativeState(DeviceHostState &state);
    // 呈现阶段入口由 DeviceHost 的线程, 重入和就绪检查统一包围.
    [[nodiscard]] DeviceHost::PresentationAcquire BeginPresentation(DeviceHostState &state, std::size_t slotIndex);
    [[nodiscard]] std::optional<HostError> PrepareSubmission(DeviceHostState &state, PresentationToken token,
                                                            RHI::CommandList &commands, bool frameSucceeded);
    [[nodiscard]] PresentationSubmission SubmitPresentation(DeviceHostState &state, PresentationToken token,
                                                            std::optional<HostError> &error);
    [[nodiscard]] PresentationResult Present(DeviceHostState &state, PresentationToken token,
                                              std::optional<HostError> &error);
    [[nodiscard]] std::optional<HostError> CancelPresentation(DeviceHostState &state, PresentationToken token);
    /// @brief 在外部 owner 预检前撤销宿主的目标, 观察者和列表持有.
    /// 已发前缀及其依赖仍由设备的原生包和退出流程保活.
    void ReleasePresentation(DeviceHostState &state) noexcept;
    /// @brief 读取初始窗口事实后启用输入回调, 再补查安装期间的状态变化.
    [[nodiscard]] std::optional<HostError> InitializeWindowInput(DeviceHostState &state);
    /// @brief 停止输入写入, 解除相对捕获并解绑回调, 合并清理错误.
    [[nodiscard]] std::optional<HostError> ReleaseWindowInput(DeviceHostState &state);
    /// @brief 请求 Normal 光标模式并查询确认, 仅确认后清除待释放标记.
    [[nodiscard]] std::optional<HostError> ReleaseRelativeCursor(DeviceHostState &state);
    /// @brief 分发原生事件并在安全边界补查事实与解除捕获, 不发布 bank.
    [[nodiscard]] std::optional<HostError> PollNativeInput(DeviceHostState &state);
    /// @brief 校验有限正秒后等待事件, 使用与 poll 相同的事实和故障收尾.
    [[nodiscard]] std::optional<HostError> WaitNativeInput(DeviceHostState &state, double timeoutSeconds);
    /// @brief 先整批预检再顺序执行平台请求, 运行期失败保留已生效的前缀.
    [[nodiscard]] std::optional<HostError> ApplyNativeRequests(DeviceHostState &state,
                                                               std::span<const HostRequest> requests);
    /// @brief 将累积器首错复制成宿主错误, 不清除已锁存的终止故障.
    [[nodiscard]] std::optional<HostError> TakeInputFault(DeviceHostState &state);

    /// @brief 仅负责私有构造权限与失败回滚的装配入口, 不增加另一份宿主状态.
    struct DeviceHostFactory
    {
        [[nodiscard]] static DeviceHost::CreateResult Create(const HostDesc &desc,
                                                             Core::Diagnostics &diagnostics) noexcept;
    };
}
