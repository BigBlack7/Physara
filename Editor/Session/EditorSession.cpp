#include "EditorSession.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include <DeviceHost.hpp>

#include "ActionRouter.hpp"
#include "ImGui/GlfwPlatform.hpp"
#include "ImGui/UiRenderer.hpp"
#include "Panels/Workspace.hpp"

/// @file
/// @brief 将宿主发布推进为一次 Editor 轮次. 先识别输入失效边界, 再按窗口状态构建布局.
/// 可渲染轮次封存 CPU 绘制数据及同批像素范围, 供可选的离屏 renderer 消费.

namespace Physara::Editor
{
    /// @brief 会话内的 context 依赖, 输入恢复状态和本轮 DrawData 借用.
    struct EditorSession::State
    {
        // 平台适配最后析构, renderer 先结束借用并解除纹理关联, 再销毁 UI context.
        Detail::GlfwPlatform platform;
        std::unique_ptr<Detail::UiRenderer> renderer;
        Detail::Workspace workspace;
        std::optional<Detail::SelectionId> selection;
        const ImDrawData *drawData = nullptr; ///< 借用本轮封存数据, 随下一次 Advance 失效.
        Platform::WindowExtent extentPixels; ///< 仅在 drawData 有效时使用, 单位为 framebuffer 像素.
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
        return CreateImpl(host, nullptr);
    }

    EditorSession::CreateResult EditorSession::Create(Platform::DeviceHost &host, const UiRendererDesc &renderer)
    {
        return CreateImpl(host, &renderer);
    }

    // 两种消费方式共享平台接入. 资源准备失败时, 会话局部 owner 负责逆序清理.
    EditorSession::CreateResult EditorSession::CreateImpl(Platform::DeviceHost &host, const UiRendererDesc *renderer)
    {
        // 录制通过宿主回调观察失败, 因此 renderer 只接入该宿主借出的 Device.
        if (renderer != nullptr && host.GetDevice() != &renderer->device)
        {
            return Core::BoundedText("Editor UI renderer requires the ready host device on its owner thread");
        }
        auto session = std::unique_ptr<EditorSession>(new EditorSession());
        auto &state = *session->m_State;
        if (auto error = state.platform.Attach(host))
        {
            return *error;
        }
        // RHI 路径由 ImGui 动态生成 atlas. CPU 路径在首帧前预建 atlas 以满足固定版本的合同.
        if (renderer != nullptr)
        {
            auto created = Detail::UiRenderer::Create(host, *renderer);
            if (const auto *error = std::get_if<Core::BoundedText>(&created))
            {
                return *error;
            }
            state.renderer = std::get<std::unique_ptr<Detail::UiRenderer>>(std::move(created));
        }
        else if (!ImGui::GetIO().Fonts->Build())
        {
            return Core::BoundedText("Editor CPU font atlas initialization failed");
        }
        return session;
    }

    EditorFrame EditorSession::Advance(const Platform::HostFrameInput &input)
    {
        auto &state = *m_State;
        if (state.renderer)
        {
            // ImGui 可在下一次 NewFrame 覆写状态或释放旧 atlas 元数据.
            state.renderer->CancelPending();
        }
        state.drawData = nullptr;
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
            // 封存时一并记住同次宿主发布的像素范围, 后续准备以它校验调用者目标.
            ImGui::Render();
            state.drawData = ImGui::GetDrawData();
            state.extentPixels = input.window.framebufferSizePixels;
            frame.hasDrawData = state.drawData != nullptr;
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

    PrepareUiResult EditorSession::PrepareUiFrame(RHI::Framebuffer *target, Platform::WindowExtent extentPixels)
    {
        auto &state = *m_State;
        // 暂停或零像素范围直接返回, 待处理纹理请求留给下一次有效准备.
        if (state.drawData == nullptr || extentPixels.width == 0 || extentPixels.height == 0)
        {
            return std::monostate{};
        }
        if (!state.renderer || extentPixels != state.extentPixels)
        {
            return Core::BoundedText("Editor UI prepare requires a renderer and the sealed frame's pixel extent");
        }
        return state.renderer->Prepare(*state.drawData, target, extentPixels);
    }

    UiTextureResult EditorSession::RegisterUiTexture(RHI::Texture &texture)
    {
        auto &renderer = m_State->renderer;
        if (!renderer)
        {
            return Core::BoundedText("Editor UI texture registration requires an attached renderer");
        }
        return renderer->RegisterResident(texture);
    }

    bool EditorSession::UnregisterUiTexture(UiTextureId id) noexcept
    {
        auto &renderer = m_State->renderer;
        return renderer && renderer->UnregisterResident(id);
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
