#include "DeviceHost.hpp"

/// @file
/// @brief 装配宿主并统一处理线程, 重入与失败收拢.
/// 输入发布和索引呈现的原生步骤由内部阶段入口执行, 此文件维持操作边界.

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <exception>
#include <limits>
#include <new>
#include <utility>

#include "DeviceHostInternal.hpp"
#include "GlfwAccess.hpp"

namespace Physara::Platform
{

    static constexpr std::array<std::string_view, 18> StageNames = {
        "Validation", "Reservation", "GlfwInitialization",
        "WindowCreation", "ContextActivation", "EntryLoading",
        "EnvironmentQuery", "SwapInterval", "Ready",
        "Shutdown", "InputSetup", "EventPolling",
        "EventWait", "RequestApplication", "RhiCreation", "RhiShutdown", "Presentation", "Unknown"};

    // 将槽索引与分配代写入同一有界正文, 不为诊断建立动态字符串.
    static void AppendIdentity(Physara::Core::BoundedText &text, WindowId identity) noexcept
    {
        std::array<char, 32> number{};
        text.Append(" window=");
        auto result = std::to_chars(number.data(), number.data() + number.size(), identity.index);
        text.Append({number.data(), static_cast<std::size_t>(result.ptr - number.data())});
        text.Append("@");
        result = std::to_chars(number.data(), number.data() + number.size(), identity.generation);
        text.Append({number.data(), static_cast<std::size_t>(result.ptr - number.data())});
    }

    // 在分配宿主和触碰 GLFW 之前校验逻辑尺寸, 标题和呈现槽容量.
    static std::optional<HostError> ValidateDescription(const HostDesc &desc) noexcept
    {
        constexpr auto MaxDimension = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
        if (desc.clientWidthLogical == 0 || desc.clientHeightLogical == 0 || desc.clientWidthLogical > MaxDimension ||
            desc.clientHeightLogical > MaxDimension || desc.title.find('\0') != std::string_view::npos)
        {
            return Detail::MakeError(nullptr, HostErrorReason::InvalidConfiguration, HostStage::Validation,
                                     "Initial logical size must be positive and fit int; title must not contain NUL");
        }
        if (desc.presentationSlotCount == 0 || desc.presentationSlotCount > MaxPresentationSlots)
        {
            return Detail::MakeError(nullptr, HostErrorReason::InvalidConfiguration, HostStage::Validation,
                                     "Presentation slot count exceeds the supported range");
        }
        return std::nullopt;
    }

    // 依次取得 GLFW, 窗口/context, 入口/环境, RHI 和输入, 首错后停止进入后继阶段.
    static std::optional<HostError> StartNativeHost(Detail::DeviceHostState &state, const HostDesc &desc)
    {
        // 固定进入顺序与统一回滚是一项完整算法, 保留在一起便于核对每个已取得阶段.
        auto error = Detail::InitializeGlfw(state);
        if (!error)
        {
            error = Detail::CreateGlfwWindow(state, desc);
        }
        if (!error)
        {
            error = Detail::ActivateContext(state);
        }
        if (!error)
        {
            error = Detail::LoadGlEntries(state);
        }
        if (!error)
        {
            error = Detail::InspectEnvironment(state);
        }
        if (!error)
        {
            state.stage = HostStage::RhiCreation;
            error = Detail::InitializeResourceDevice(state);
        }
        if (!error)
        {
            error = Detail::RequestSwapInterval(state, desc.vsync);
        }
        if (!error)
        {
            error = Detail::InitializeWindowInput(state);
        }
        return error;
    }

    // Ready 日志与环境条目都携带同一窗口代, 便于区分关闭后重建的宿主.
    static void ReportReady(Physara::Core::Diagnostics &diagnostics, const HostInfo &info) noexcept
    {
        Physara::Core::BoundedText message("DeviceHost.Ready");
        AppendIdentity(message, info.identity);
        static_cast<void>(diagnostics.Write(Physara::Core::Severity::Info, message.View()));
        const std::array<std::pair<std::string_view, std::string_view>, 4> environment = {
            {{"vendor", info.vendor.View()},
             {"renderer", info.renderer.View()},
             {"version", info.version.View()},
             {"GLSL", info.shadingLanguageVersion.View()}}};
        for (const auto &[name, text] : environment)
        {
            Physara::Core::BoundedText value("DeviceHost.Environment");
            AppendIdentity(value, info.identity);
            value.Append(" ");
            value.Append(name);
            value.Append("=");
            value.Append(text);
            static_cast<void>(diagnostics.Write(Physara::Core::Severity::Info, value.View()));
        }
    }
    // 已完成线程/重入预检后的作用域标记, 异常退出也恢复 operationActive.
    class OperationScope final
    {
    public:
        OperationScope(Detail::DeviceHostState &state, HostStage stage) noexcept : m_State(state)
        {
            m_State.operationActive = true;
            m_State.stage = stage;
        }
        ~OperationScope()
        {
            m_State.operationActive = false;
        }
        OperationScope(const OperationScope &) = delete;
        OperationScope &operator=(const OperationScope &) = delete;
        OperationScope(OperationScope &&) = delete;
        OperationScope &operator=(OperationScope &&) = delete;

    private:
        Detail::DeviceHostState &m_State;
    };

    // 输入和呈现共用线程及重入检查. 身份或配置拒绝可恢复, 操作故障锁存并解除光标捕获.
    template <typename Operation>
    static std::optional<HostError> RunHostOperation(Detail::DeviceHostState &state, HostStage stage,
                                                      Operation &&operation) noexcept
    {
        // 先拒绝错误线程/重入/停止状态, 再进入原生边界. 参数预检拒绝可恢复, 输入或原生故障则终止该输入寿命.
        const auto started = std::chrono::steady_clock::now();
        const auto reject = [&](HostErrorReason reason, std::string_view message)
        {
            auto error = Detail::MakeError(&state, reason, stage, message);
            error.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            return error;
        };
        if (state.ownerThread != std::this_thread::get_id())
        {
            return reject(HostErrorReason::WrongThread, "Host operation requires the owner thread");
        }
        if (state.operationActive)
        {
            return reject(HostErrorReason::ReentrantCall, "Native host operation is not reentrant");
        }
        if (state.stopped)
        {
            return reject(HostErrorReason::Stopped, "Host operation requires a live host");
        }
        if (state.inputFailure)
        {
            return state.inputFailure;
        }
        if (!state.ready)
        {
            return reject(HostErrorReason::Stopped, "Host is not ready for operations");
        }
        OperationScope scope(state, stage);
        std::optional<HostError> error;
        try
        {
            error = operation();
        }
        catch (const std::exception &failure)
        {
            error = Detail::MakeError(&state, HostErrorReason::InternalFailure, stage, failure.what());
        }
        catch (...)
        {
            error =
                Detail::MakeError(&state, HostErrorReason::InternalFailure, stage, "Unknown host operation failure");
        }
        if (!error)
        {
            return std::nullopt;
        }
        const bool rejected = (error->reason == HostErrorReason::InvalidConfiguration ||
                               error->reason == HostErrorReason::RequestRejected) &&
                              !error->cleanupError;
        if (!rejected)
        {
            // 操作故障关闭就绪入口. 先禁回调写入并保存主错, 再解除光标捕获.
            // 已发图形工作及其引用保留给关闭路径收拢.
            state.ready = false;
            state.inputAccepting = false;
            if (error->context.nativeCode)
            {
                if (auto input = Detail::TakeInputFault(state))
                {
                    error->context.description.Append("; ");
                    error->context.description.Append(input->context.description.View());
                }
            }
            state.input.ResetForFailure();
            // 清理也可能同步触发窗口回调; 先停止输入入口, 全局错误捕获仍保持有效.
            state.inputFailure = error;
            try
            {
                if (auto cleanup = Detail::ReleaseRelativeCursor(state))
                {
                    Detail::AttachCleanupFailure(*error, *cleanup);
                }
            }
            catch (...)
            {
                Detail::TerminateForContract("Input failure cleanup synchronization failed");
            }
        }
        error->elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (!rejected)
        {
            state.inputFailure = error;
        }
        Detail::ReportError(state.diagnostics, *error);
        return error;
    }

}

namespace Physara::Platform::Detail
{
    HostError MakeError(const DeviceHostState *state, HostErrorReason reason, HostStage stage, std::string_view message,
                        std::source_location source) noexcept
    {
        HostError error;
        error.reason = reason;
        error.stage = stage;
        error.context = {Core::BoundedText(message), std::nullopt, source};
        if (state != nullptr)
        {
            if (state->info.identity.IsValid())
            {
                error.attempt = state->info.identity;
            }
            error.elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - state->started).count();
        }
        return error;
    }

    void ReportError(Core::Diagnostics &diagnostics, const HostError &error) noexcept
    {
        const auto text = DescribeHostError(error);
        static_cast<void>(diagnostics.Write(Core::Severity::Error, text.View(), error.context.source));
    }

    void AttachCleanupFailure(HostError &primary, const HostError &cleanup) noexcept
    {
        if (!primary.cleanupError)
        {
            primary.cleanupError = cleanup.context;
            primary.additionalCleanupErrors = cleanup.additionalNativeErrors;
        }
        else
        {
            constexpr auto max = std::numeric_limits<std::uint64_t>::max();
            const auto count = cleanup.additionalNativeErrors == max ? max : cleanup.additionalNativeErrors + 1;
            primary.additionalCleanupErrors += std::min(count, max - primary.additionalCleanupErrors);
        }
    }

    void MergeCleanupError(std::optional<HostError> &first, std::optional<HostError> next) noexcept
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

    [[noreturn]] void TerminateForContract(std::string_view message) noexcept
    {
        static_cast<void>(std::fwrite(message.data(), 1, message.size(), stderr));
        static_cast<void>(std::fwrite("\n", 1, 1, stderr));
        std::terminate();
    }

    DeviceHost::CreateResult DeviceHostFactory::Create(const HostDesc &desc, Core::Diagnostics &diagnostics) noexcept
    {
        const auto started = std::chrono::steady_clock::now();
        std::unique_ptr<DeviceHost> host;
        // 所有阶段失败都经过同一逆序释放入口, 保留首错并附上清理故障.
        const auto fail = [&](HostError error) -> DeviceHost::CreateResult
        {
            if (host)
            {
                if (auto cleanup = ReleaseNativeState(*host->m_State))
                {
                    AttachCleanupFailure(error, *cleanup);
                }
            }
            error.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            ReportError(diagnostics, error);
            return error;
        };
        try
        {
            if (auto error = ValidateDescription(desc))
            {
                return fail(*error);
            }
            auto state = std::make_unique<DeviceHostState>(diagnostics);
            state->started = started;
            state->presentation.slotCount = desc.presentationSlotCount;
            host = std::unique_ptr<DeviceHost>(new DeviceHost(std::move(state)));
            if (auto error = ReserveProcess(*host->m_State))
            {
                return fail(*error);
            }
            if (auto error = StartNativeHost(*host->m_State, desc))
            {
                return fail(*error);
            }
            host->m_State->info.creationSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            host->m_State->stage = HostStage::Ready;
            host->m_State->ready = true;
            ReportReady(diagnostics, host->m_State->info);
            return host;
        }
        catch (const std::bad_alloc &)
        {
            return fail(MakeError(host ? host->m_State.get() : nullptr, HostErrorReason::OutOfMemory,
                                  host ? host->m_State->stage : HostStage::Reservation, "Host allocation failed"));
        }
        catch (const std::exception &error)
        {
            return fail(MakeError(host ? host->m_State.get() : nullptr, HostErrorReason::InternalFailure,
                                  host ? host->m_State->stage : HostStage::Reservation, error.what()));
        }
        catch (...)
        {
            return fail(MakeError(host ? host->m_State.get() : nullptr, HostErrorReason::InternalFailure,
                                  host ? host->m_State->stage : HostStage::Reservation,
                                  "Unknown host initialization failure"));
        }
    }

    GLFWwindow *GlfwAccess::Borrow(DeviceHost &host) noexcept
    {
        const auto &state = *host.m_State;
        if (state.ownerThread != std::this_thread::get_id() || !state.ready)
        {
            return nullptr;
        }
        return state.window;
    }

}

namespace Physara::Platform
{
    Core::BoundedText DescribeHostError(const HostError &error) noexcept
    {
        const auto stageIndex = static_cast<std::size_t>(error.stage);
        Core::BoundedText message("DeviceHost.");
        message.Append(StageNames[stageIndex < StageNames.size() - 1 ? stageIndex : StageNames.size() - 1]);
        if (error.attempt)
        {
            AppendIdentity(message, *error.attempt);
        }
        if (error.context.nativeCode)
        {
            message.Append(error.context.nativeCode->domain == Core::NativeErrorDomain::Glfw ? " GLFW=" : " GL=");
            std::array<char, 32> number{};
            const auto result =
                std::to_chars(number.data(), number.data() + number.size(), error.context.nativeCode->value);
            message.Append({number.data(), static_cast<std::size_t>(result.ptr - number.data())});
        }
        if (error.requestIndex)
        {
            std::array<char, 32> number{};
            const auto result = std::to_chars(number.data(), number.data() + number.size(), *error.requestIndex);
            message.Append(" request=");
            message.Append({number.data(), static_cast<std::size_t>(result.ptr - number.data())});
        }
        message.Append(": ");
        message.Append(error.context.description.View());
        if (error.cleanupError)
        {
            message.Append("; cleanup: ");
            message.Append(error.cleanupError->description.View());
        }
        return message;
    }

    DeviceHost::DeviceHost(std::unique_ptr<Detail::DeviceHostState> state) noexcept : m_State(std::move(state))
    {
    }

    DeviceHost::~DeviceHost()
    {
        if (m_State->ownerThread != std::this_thread::get_id())
        {
            Detail::TerminateForContract("DeviceHost destroyed on a non-owner thread");
        }
        if (m_State->operationActive)
        {
            Detail::TerminateForContract("DeviceHost destroyed during an active native operation");
        }
        static_cast<void>(Shutdown());
        if (!m_State->stopped)
        {
            Detail::TerminateForContract("DeviceHost destroyed with external RHI owners or an unfinished RHI drain");
        }
    }

    DeviceHost::CreateResult DeviceHost::Create(const HostDesc &desc, Core::Diagnostics &diagnostics) noexcept
    {
        return Detail::DeviceHostFactory::Create(desc, diagnostics);
    }

    std::optional<HostError> DeviceHost::Shutdown() noexcept
    {
        if (m_State->ownerThread != std::this_thread::get_id())
        {
            return Detail::MakeError(m_State.get(), HostErrorReason::WrongThread, HostStage::Shutdown,
                                     "Shutdown requires the host owner thread");
        }
        if (m_State->operationActive)
        {
            return Detail::MakeError(m_State.get(), HostErrorReason::ReentrantCall, HostStage::Shutdown,
                                     "Shutdown is not reentrant");
        }
        OperationScope scope(*m_State, HostStage::Shutdown);
        try
        {
            auto error = Detail::ReleaseNativeState(*m_State);
            if (error)
            {
                Detail::ReportError(m_State->diagnostics, *error);
            }
            return error;
        }
        catch (...)
        {
            Detail::TerminateForContract("DeviceHost release synchronization failed");
        }
    }

    bool DeviceHost::IsReady() const noexcept
    {
        return m_State->ready;
    }

    HostInfo DeviceHost::GetInfo() const noexcept
    {
        return m_State->info;
    }

    RHI::Device *DeviceHost::GetDevice() const noexcept
    {
        if (m_State->ownerThread != std::this_thread::get_id() || !m_State->ready)
        {
            return nullptr;
        }
        return m_State->validatedDevice.Get();
    }

    bool RhiErrorScope::IsClear() const noexcept
    {
        // 同步回调在命令返回前更新计数. 区间只比较这一原子值, 使用 relaxed 读取即可.
        return m_ErrorCount != nullptr && m_InitialCount != std::numeric_limits<std::uint64_t>::max() &&
               m_ErrorCount->load(std::memory_order_relaxed) == m_InitialCount;
    }

    RhiErrorScope DeviceHost::CaptureRhiErrors() const noexcept
    {
        RhiErrorScope scope;
        if (GetDevice() != nullptr)
        {
            scope.m_ErrorCount = &m_State->rhiMessages.ErrorCount();
            scope.m_InitialCount = scope.m_ErrorCount->load(std::memory_order_relaxed);
        }
        return scope;
    }

    // 呈现包装层统一执行宿主操作检查. Suspended 和 Retry 沿值结果返回, 域错误走宿主诊断.
    DeviceHost::PresentationAcquire DeviceHost::BeginPresentation(std::size_t slotIndex) noexcept
    {
        PresentationAcquire result = PresentationStatus::Retry;
        auto error = RunHostOperation(*m_State, HostStage::Presentation, [&]() -> std::optional<HostError>
        {
            result = Detail::BeginPresentation(*m_State, slotIndex);
            if (const auto *failure = std::get_if<HostError>(&result))
            {
                return *failure;
            }
            return std::nullopt;
        });
        return error ? PresentationAcquire{*error} : std::move(result);
    }

    std::optional<HostError> DeviceHost::PrepareSubmission(PresentationToken token, RHI::CommandList &commands,
                                                         bool frameSucceeded) noexcept
    {
        return RunHostOperation(*m_State, HostStage::Presentation,
                                [&] { return Detail::PrepareSubmission(*m_State, token, commands, frameSucceeded); });
    }

    // 错误经统一操作边界报告. 返回值独立保存 Execute 已接受的票据, 包括观察绑定失败时.
    PresentationSubmission DeviceHost::SubmitPresentation(PresentationToken token,
                                                          std::optional<HostError> &error) noexcept
    {
        PresentationSubmission result;
        error = RunHostOperation(*m_State, HostStage::Presentation, [&]() -> std::optional<HostError>
        {
            std::optional<HostError> failure;
            result = Detail::SubmitPresentation(*m_State, token, failure);
            return failure;
        });
        return result;
    }

    PresentationResult DeviceHost::Present(PresentationToken token, std::optional<HostError> &error) noexcept
    {
        // 交换错误经宿主边界锁存. 已接受的票据和实际交换区间仍通过 result 交付.
        PresentationResult result;
        error = RunHostOperation(*m_State, HostStage::Presentation, [&]() -> std::optional<HostError>
        {
            std::optional<HostError> failure;
            result = Detail::Present(*m_State, token, failure);
            return failure;
        });
        return result;
    }

    std::optional<HostError> DeviceHost::CancelPresentation(PresentationToken token) noexcept
    {
        return RunHostOperation(*m_State, HostStage::Presentation,
                                [&] { return Detail::CancelPresentation(*m_State, token); });
    }

    DeviceHost::PollResult DeviceHost::PollEvents() noexcept
    {
        // 先完成原生事件分发及事实补查, 再一次发布. 任何失败都返回 HostError, 不暴露部分事件 bank.
        std::optional<HostFrameInput> frame;
        auto error = RunHostOperation(*m_State, HostStage::EventPolling,
                                       [&]() -> std::optional<HostError>
                                       {
                                           if (auto failure = Detail::PollNativeInput(*m_State))
                                           {
                                               return failure;
                                           }
                                           frame = m_State->input.Publish();
                                           if (!frame)
                                           {
                                               if (auto failure = Detail::TakeInputFault(*m_State))
                                               {
                                                   return failure;
                                               }
                                               return Detail::MakeError(m_State.get(), HostErrorReason::InternalFailure,
                                                                        HostStage::EventPolling,
                                                                        "Publication failed without input fault");
                                           }
                                           return std::nullopt;
                                       });
        if (error)
        {
            return *error;
        }
        return *frame;
    }
    std::optional<HostError> DeviceHost::WaitForEvents(double timeoutSeconds) noexcept
    {
        return RunHostOperation(*m_State, HostStage::EventWait,
                                 [&]
                                 { return Detail::WaitNativeInput(*m_State, timeoutSeconds); });
    }
    std::optional<HostError> DeviceHost::ApplyDeferredRequests(std::span<const HostRequest> requests) noexcept
    {
        return RunHostOperation(*m_State, HostStage::RequestApplication,
                                 [&]
                                 { return Detail::ApplyNativeRequests(*m_State, requests); });
    }
}
