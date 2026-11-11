#include "DeviceHostInternal.hpp"

/// @file
/// @brief 推进单槽目标的取得, 末端复制, 接受提交和交换.
/// 宿主保存颜色资格和 Event 快照. 源依赖及已发前缀随设备的物理包退休.

#include <limits>
#include <array>
#include <charconv>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <OpenGL.hpp>
#include <Validation/CommandListInterop.hpp>

namespace Physara::Platform::Detail
{
    // 附上宿主和呈现阶段. 原始 RHI 诊断由宿主的消息适配器同步记录.
    static HostError PresentationError(DeviceHostState &state, std::string_view message)
    {
        return MakeError(&state, HostErrorReason::PresentationFailed, HostStage::Presentation, message);
    }

    // 饱和序号无法再证明区间无错, 与公开 RhiErrorScope 使用相同的取样规则.
    static bool RhiSucceeded(const DeviceHostState &state, std::uint64_t before) noexcept
    {
        return before != std::numeric_limits<std::uint64_t>::max() &&
               state.rhiMessages.ErrorCount().load(std::memory_order_relaxed) == before;
    }

    // 同一目标仍可能因窗口系统异步变化失效, 只在末端消费和交换边界重查原生像素事实.
    static std::variant<WindowExtent, HostError> CurrentWindowExtent(DeviceHostState &state)
    {
        BeginNativeOperation(state);
        int width = 0, height = 0;
        glfwGetFramebufferSize(state.window, &width, &height);
        const bool suspended = glfwGetWindowAttrib(state.window, GLFW_ICONIFIED) == GLFW_TRUE ||
                               glfwWindowShouldClose(state.window) == GLFW_TRUE;
        if (auto error = TakeNativeError(state, HostErrorReason::PresentationFailed, HostStage::Presentation))
        {
            return *error;
        }
        if (width < 0 || height < 0)
        {
            return PresentationError(state, "Invalid native presentation extent");
        }
        return suspended ? WindowExtent{} : WindowExtent{std::uint32_t(width), std::uint32_t(height)};
    }

    // 身份拒绝不消费当前取得, 允许调用者用正确 token 继续或取消.
    static std::optional<HostError> RequirePresentation(DeviceHostState &state, PresentationToken token,
                                                        PresentationPhase phase)
    {
        if (state.presentation.phase != phase || token != state.presentation.token)
        {
            return MakeError(&state, HostErrorReason::RequestRejected, HostStage::Presentation,
                             "Presentation token or operation phase is invalid");
        }
        return std::nullopt;
    }

    // 先按窗口事实决定是否取得, 再确认上一笔完成. 只在槽空闲且尺寸变化时替换整组目标.
    DeviceHost::PresentationAcquire BeginPresentation(DeviceHostState &state)
    {
        auto &slot = state.presentation;
        if (slot.stopped || slot.phase != PresentationPhase::Idle)
        {
            return MakeError(&state, HostErrorReason::RequestRejected, HostStage::Presentation,
                             "Presentation is stopped or already acquired");
        }
        const auto &window = state.input.Window();
        if (!window.Renderable())
        {
            return PresentationStatus::Suspended;
        }
        // Event 轮询的 false 可能是正常 Pending, 也可能伴随诊断. 先判诊断再决定能否重试.
        const auto before = state.rhiMessages.ErrorCount().load(std::memory_order_relaxed);
        auto &device = *state.validatedDevice;
        if (slot.lastSubmission != 0)
        {
            const bool complete = device.PollEventQuery(slot.event);
            if (!RhiSucceeded(state, before))
            {
                slot.stopped = true;
                return PresentationError(state, "Presentation completion observation failed");
            }
            if (!complete)
            {
                return PresentationStatus::Retry;
            }
            slot.lastSubmission = 0;
        }
        if (slot.acquisition == std::numeric_limits<std::uint64_t>::max())
        {
            return PresentationError(state, "Presentation acquisition identity exhausted");
        }
        if (!slot.target.texture || slot.target.extentPixels != window.framebufferSizePixels)
        {
            if (slot.targetGeneration == std::numeric_limits<std::uint64_t>::max())
            {
                return PresentationError(state, "Presentation target generation exhausted");
            }
            RHI::TextureDesc desc;
            desc.width = window.framebufferSizePixels.width;
            desc.height = window.framebufferSizePixels.height;
            desc.format = RHI::Format::SRGBA8_UNORM;
            desc.isRenderTarget = true;
            desc.initialState = RHI::ResourceStates::ShaderResource;
            desc.keepInitialState = true;
            desc.debugName = "PresentColor";
            // 局部句柄收拢部分创建. 纹理, FBO 和观察者全部就绪后才发布新目标代.
            PresentTarget candidate;
            candidate.texture = device.CreateTexture(desc);
            if (candidate.texture)
            {
                candidate.framebuffer = device.CreateFramebuffer(
                    RHI::FramebufferDesc{}.AddColorAttachment(candidate.texture));
            }
            auto event = slot.event ? slot.event : device.CreateEventQuery();
            if (!candidate.framebuffer || !event || !RhiSucceeded(state, before))
            {
                return PresentationError(state, "Presentation target or completion observer creation failed");
            }
            candidate.extentPixels = window.framebufferSizePixels;
            slot.target = std::move(candidate);
            slot.event = std::move(event);
            ++slot.targetGeneration;
        }
        slot.token = {state.info.identity, slot.targetGeneration, ++slot.acquisition};
        slot.phase = PresentationPhase::Acquired;
        slot.colorValid = false;
        return PresentationFrame{slot.target, slot.token};
    }

    // 验证取得和包装列表后锁定提交对象. 失败帧与过期窗口仍可提交合法前缀.
    std::optional<HostError> PrepareSubmission(DeviceHostState &state, PresentationToken token,
                                               RHI::CommandList &commands, bool frameSucceeded)
    {
        if (auto error = RequirePresentation(state, token, PresentationPhase::Acquired))
        {
            return error;
        }
        auto *native = RHI::Validation::BorrowClosedCommandList(state.validatedDevice, &commands);
        if (!native)
        {
            return MakeError(&state, HostErrorReason::RequestRejected, HostStage::Presentation,
                             "Presentation requires the same device's closed validated Graphics list");
        }
        auto &slot = state.presentation;
        slot.commands = &commands;
        slot.phase = PresentationPhase::Prepared;
        // 从这里起, 关联列表随槽持有. 只有通过窗口复查和原生复制才发布有效颜色.
        if (!frameSucceeded)
        {
            return std::nullopt;
        }
        const auto current = CurrentWindowExtent(state);
        if (const auto *error = std::get_if<HostError>(&current))
        {
            return *error;
        }
        if (std::get<WindowExtent>(current) != slot.target.extentPixels)
        {
            return std::nullopt;
        }
        slot.colorValid = RHI::OpenGL::PrepareWindowPresentation(
            state.rawDevice, native, slot.target.framebuffer,
            state.info.defaultFramebufferEncoding == FramebufferEncoding::SRGB);
        if (!slot.colorValid)
        {
            // 可能已有前缀和末端访问. 保留关联列表, 交宿主关闭路径收拢原生包.
            slot.stopped = true;
            return PresentationError(state, "Native terminal window copy failed");
        }
        return std::nullopt;
    }

    // 验证层 Execute 接受闭合列表后, 立即将该 Graphics 前缀绑定到槽的完成观察.
    std::variant<PresentationSubmission, HostError> SubmitPresentation(DeviceHostState &state, PresentationToken token)
    {
        if (auto error = RequirePresentation(state, token, PresentationPhase::Prepared))
        {
            return *error;
        }
        auto &slot = state.presentation;
        const auto before = state.rhiMessages.ErrorCount().load(std::memory_order_relaxed);
        const auto ticket = state.validatedDevice->ExecuteCommandList(slot.commands);
        // 已接受的票据先入槽. 随后的同步诊断和 Event 设置仍可能使本帧退出.
        slot.lastSubmission = ticket;
        if (!ticket || !RhiSucceeded(state, before))
        {
            slot.stopped = true;
            slot.colorValid = false;
            return PresentationError(state, "Presentation submission was not accepted cleanly");
        }
        // 快照在任何后续提交之前绑定刚接受的票据. 失败时仍保留真实提交, 只停止复用.
        state.validatedDevice->ResetEventQuery(slot.event);
        state.validatedDevice->SetEventQuery(slot.event, RHI::CommandQueue::Graphics);
        if (!RhiSucceeded(state, before))
        {
            slot.stopped = true;
            return PresentationError(state, "Accepted presentation has no reliable completion observer");
        }
        slot.commands.Reset();
        slot.phase = PresentationPhase::Submitted;
        return PresentationSubmission{ticket, slot.colorValid};
    }

    // 结束已提交的取得, 在最后一次窗口事实和 context 校验后交换, 同时返回原接受票据.
    PresentationResult Present(DeviceHostState &state, PresentationToken token, std::optional<HostError> &error)
    {
        PresentationResult result;
        error = RequirePresentation(state, token, PresentationPhase::Submitted);
        if (error)
        {
            return result;
        }
        auto &slot = state.presentation;
        result.ticket = slot.lastSubmission;
        const bool validColor = slot.colorValid;
        // 一旦匹配取得即消费交换资格. 是否实际尝试交换独立于槽内提交的完成观察.
        slot.phase = PresentationPhase::Idle;
        slot.colorValid = false;
        if (!validColor)
        {
            return result;
        }
        const auto current = CurrentWindowExtent(state);
        if (const auto *failure = std::get_if<HostError>(&current))
        {
            error = *failure;
            return result;
        }
        if (std::get<WindowExtent>(current) != slot.target.extentPixels)
        {
            return result;
        }
        if (glfwGetCurrentContext() != state.window)
        {
            error = PresentationError(state, "The owning presentation context is not current");
            return result;
        }
        // 从当前 WGL context 借用 DC, 其寿命仍由 GLFW 窗口管理.
        const auto dc = wglGetCurrentDC();
        if (!dc)
        {
            error = PresentationError(state, "The current presentation context has no device context");
            return result;
        }
        const auto started = std::chrono::steady_clock::now();
        result.attempted = true;
        // 固定 GLFW 的 WGL 实现丢弃 SwapBuffers 返回值, 此处检查实际 Win32 结果并保留接受票据.
        const auto swapped = SwapBuffers(dc);
        // 失败时立即取线程错误码, 保留本次调用的结果和耗时.
        const auto nativeError = swapped ? ERROR_SUCCESS : GetLastError();
        result.presentWaitSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        result.swapped = swapped != FALSE;
        if (!result.swapped)
        {
            error = PresentationError(state, "Win32 SwapBuffers failed, error=");
            std::array<char, 24> text{};
            const auto converted = std::to_chars(text.data(), text.data() + text.size(), nativeError);
            error->context.description.Append({text.data(), static_cast<std::size_t>(converted.ptr - text.data())});
        }
        return result;
    }

    // 准备阶段尚未关联列表时退还取得. 下次取得仍以新的单次身份发布.
    std::optional<HostError> CancelPresentation(DeviceHostState &state, PresentationToken token)
    {
        if (auto error = RequirePresentation(state, token, PresentationPhase::Acquired))
        {
            return error;
        }
        state.presentation.phase = PresentationPhase::Idle;
        return std::nullopt;
    }

    // 释放宿主强持有, 使关闭预检只看到外部 owner 和设备内部 lease.
    void ReleasePresentation(DeviceHostState &state) noexcept
    {
        auto &slot = state.presentation;
        slot.commands.Reset();
        slot.target = {};
        slot.event.Reset();
        slot.phase = PresentationPhase::Idle;
        slot.stopped = true;
    }
}
