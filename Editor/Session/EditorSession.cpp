#include "EditorSession.hpp"

#include <algorithm>

#include <imgui.h>

#include "ActionRouter.hpp"
#include "ImGui/GlfwPlatform.hpp"
#include "Panels/Workspace.hpp"

/// @file
/// @brief 将宿主发布推进为一次 Editor 轮次. 先识别输入失效边界, 再按窗口状态构建布局.
/// 可渲染轮次封存 CPU 绘制数据, 宿主请求随结果按值交给 Runtime.

namespace Physara::Editor
{
    /// @brief 会话的 UI 状态及两种上一轮状态, 分别用于输入门控和恢复时钟.
    struct EditorSession::State
    {
        // 平台适配最后析构, 使 UI context 覆盖工作区和选择状态的寿命.
        Detail::GlfwPlatform platform;
        Detail::Workspace workspace;
        std::optional<Detail::SelectionId> selection;
        // 上一轮是否可接收交互, 与本批次的窗口事实合用.
        bool inputAvailable = true;
        // 上一轮是否跳过 UI 帧, 用于恢复时重建时间基线.
        bool suspended = false;
    };

    EditorSession::EditorSession() : m_State(std::make_unique<State>())
    {
    }

    EditorSession::~EditorSession() = default;

    EditorSession::CreateResult EditorSession::Create(Platform::DeviceHost &host)
    {
        // 由会话先持有适配的清理责任. 接入返回错误时局部 owner 随即析构.
        auto session = std::unique_ptr<EditorSession>(new EditorSession());
        if (auto error = session->m_State->platform.Attach(host))
        {
            return *error;
        }
        return session;
    }

    EditorFrame EditorSession::Advance(const Platform::HostFrameInput &input)
    {
        auto &state = *m_State;
        const bool renderable = input.window.Renderable();
        const bool inputAvailable = renderable && input.window.focused;
        // 即使发布末值已恢复, 批次中的失焦, 最小化或零像素边沿仍会使本批失效.
        // 前一批已停用时, 恢复批次仍先清理队列, 后续批次才重新派发动作.
        const bool discard = !inputAvailable || !state.inputAvailable ||
                             std::ranges::any_of(input.events, Detail::InterruptsInput);
        EditorFrame frame;
        if (discard)
        {
            state.platform.DiscardInput(input.window.focused);
            // 宿主会处理失焦时的捕获释放. 此处只在发布仍显示相对模式时请求归还.
            if (input.state.cursorMode == Platform::CursorMode::Relative)
            {
                frame.hostRequest = Platform::SetCursorMode{Platform::CursorMode::Normal};
            }
        }
        if (renderable)
        {
            // 失效轮次仍可生成 CPU 帧, 动作派发另受 discard 控制.
            state.platform.BeginFrame(input.window, state.suspended);
            constexpr auto allPopups = ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel;
            const bool keyboardOwnedBeforeLayout = ImGui::IsPopupOpen(nullptr, allPopups) || ImGui::IsAnyItemActive();
            const bool viewportHit = state.workspace.Draw(state.selection);
            if (!discard)
            {
                // 布局可释放弹出层或活动项. 已被占用的本批键盘输入不能随后触发工作区动作.
                const bool activeItem = ImGui::IsAnyItemActive();
                const Detail::InputClaims claims{
                    .blockingPopup = ImGui::IsPopupOpen(nullptr, allPopups),
                    .uiKeyboard = keyboardOwnedBeforeLayout || activeItem,
                    .uiPointer = activeItem || (!viewportHit && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)),
                    .cursorMode = input.state.cursorMode,
                    .viewportHit = viewportHit};
                state.workspace.ApplyShortcuts(input.events, claims);
            }
            ImGui::Render();
            frame.hasDrawData = ImGui::GetDrawData() != nullptr;
        }
        // 普通模式的恢复批次可在本轮结束时建立基线. 相对模式保持门控, 等宿主确认归还.
        if (discard && inputAvailable && !frame.hostRequest)
        {
            state.platform.ResumeInput();
        }
        state.inputAvailable = inputAvailable;
        state.suspended = !renderable;
        return frame;
    }

    void EditorSession::CompleteCursorRelease()
    {
        auto &state = *m_State;
        if (state.inputAvailable)
        {
            state.platform.ResumeInput();
        }
    }
}
