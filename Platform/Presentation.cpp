#include "DeviceHostInternal.hpp"

/// @file
/// @brief 在宿主线程推进一次索引取得, 末端复制, 提交和交换.
/// 活动取得保存颜色资格. 各槽的目标与 Event 独立保留, 已发访问沿设备的物理包退休.

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
    DeviceHost::PresentationAcquire BeginPresentation(DeviceHostState &state, std::size_t slotIndex)
    {
        auto &presentation = state.presentation;
        if (slotIndex >= presentation.slotCount)
        {
            return MakeError(&state, HostErrorReason::RequestRejected, HostStage::Presentation,
                             "Presentation slot index is out of range");
        }
        if (presentation.stopped || presentation.phase != PresentationPhase::Idle)
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
        auto &slot = presentation.slots[slotIndex];
        if (slot.lastSubmission != 0)
        {
            const bool complete = device.PollEventQuery(slot.event);
            if (!RhiSucceeded(state, before))
            {
                presentation.stopped = true;
                return PresentationError(state, "Presentation completion observation failed");
            }
            if (!complete)
            {
                return PresentationStatus::Retry;
            }
            slot.lastSubmission = 0;
        }
        if (presentation.acquisition == std::numeric_limits<std::uint64_t>::max())
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
            // 局部句柄收拢部分创建. 槽已有 Event 时复用, 纹理和 FBO 随目标代成组发布.
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
        presentation.token = {state.info.identity, slotIndex, slot.targetGeneration, ++presentation.acquisition};
        presentation.phase = PresentationPhase::Acquired;
        presentation.colorValid = false;
        return PresentationFrame{slot.target, presentation.token};
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
        auto &presentation = state.presentation;
        const auto &target = presentation.slots[token.slotIndex].target;
        presentation.commands = &commands;
        presentation.phase = PresentationPhase::Prepared;
        // 活动取得先强持列表. 窗口变更时保留列表供提交已发前缀, 有效颜色还需末端复制成功.
        if (!frameSucceeded)
        {
            return std::nullopt;
        }
        const auto current = CurrentWindowExtent(state);
        if (const auto *error = std::get_if<HostError>(&current))
        {
            return *error;
        }
        if (std::get<WindowExtent>(current) != target.extentPixels)
        {
            return std::nullopt;
        }
        presentation.colorValid = RHI::OpenGL::PrepareWindowPresentation(
            state.rawDevice, native, target.framebuffer,
            state.info.defaultFramebufferEncoding == FramebufferEncoding::SRGB);
        if (!presentation.colorValid)
        {
            // 可能已有前缀和末端访问. 保留关联列表, 交宿主关闭路径收拢原生包.
            presentation.stopped = true;
            return PresentationError(state, "Native terminal window copy failed");
        }
        return std::nullopt;
    }

    // 验证层 Execute 接受闭合列表后, 立即将该 Graphics 前缀绑定到槽的完成观察.
    PresentationSubmission SubmitPresentation(DeviceHostState &state, PresentationToken token,
                                              std::optional<HostError> &error)
    {
        error = RequirePresentation(state, token, PresentationPhase::Prepared);
        if (error)
        {
            return {};
        }
        auto &presentation = state.presentation;
        auto &slot = presentation.slots[token.slotIndex];
        const auto before = state.rhiMessages.ErrorCount().load(std::memory_order_relaxed);
        const auto ticket = state.validatedDevice->ExecuteCommandList(presentation.commands);
        // 已接受的票据先入槽. 随后的同步诊断和 Event 设置仍可能使本帧退出.
        slot.lastSubmission = ticket;
        if (!ticket || !RhiSucceeded(state, before))
        {
            presentation.stopped = true;
            presentation.colorValid = false;
            error = PresentationError(state, "Presentation submission was not accepted cleanly");
            return {ticket, false};
        }
        // Reset 只清旧观察身份. 同一 owner 紧接 Execute 捕获队列尾, 该快照覆盖刚接受的票据.
        // 绑定失败仍返回票据, 同时停止复用并撤销颜色资格.
        state.validatedDevice->ResetEventQuery(slot.event);
        if (RhiSucceeded(state, before))
        {
            state.validatedDevice->SetEventQuery(slot.event, RHI::CommandQueue::Graphics);
        }
        if (!RhiSucceeded(state, before))
        {
            presentation.stopped = true;
            presentation.colorValid = false;
            error = PresentationError(state, "Accepted presentation has no reliable completion observer");
            return {ticket, false};
        }
        presentation.commands.Reset();
        presentation.phase = PresentationPhase::Submitted;
        return {ticket, presentation.colorValid};
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
        auto &presentation = state.presentation;
        const auto &slot = presentation.slots[token.slotIndex];
        result.ticket = slot.lastSubmission;
        const bool validColor = presentation.colorValid;
        // 一旦匹配取得即消费交换资格. 槽的 Event 与票据继续留到后续取得, 交换只观测窗口结果.
        presentation.phase = PresentationPhase::Idle;
        presentation.colorValid = false;
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
        auto &presentation = state.presentation;
        presentation.commands.Reset();
        for (auto &slot : presentation.slots)
        {
            slot.target = {};
            slot.event.Reset();
        }
        presentation.phase = PresentationPhase::Idle;
        presentation.stopped = true;
    }
}
