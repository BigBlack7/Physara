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
/// @brief 将宿主发布推进为一次 Editor 轮次. 先处理输入失效边沿, 再封存布局和 DrawData.
/// 编辑历史在封存后应用字段意图, 视口像素请求采用同批度量与此时的 View 请求.

namespace Physara::Editor
{
    /// @brief 聚合本会话的布局, 编辑与纹理身份, 以及一轮内有效的 UI 借用.
    struct EditorSession::State
    {
        // 平台适配最后析构, renderer 先结束借用并解除纹理关联, 再销毁 UI context.
        Detail::GlfwPlatform platform;
        std::unique_ptr<Detail::UiRenderer> renderer;
        Detail::Workspace workspace;
        Detail::Viewport viewport;
        // 编辑状态在平台适配接入成功后建立, 结束会话时释放事务与历史.
        std::optional<Detail::EditHistory> edits;
        std::optional<Core::BoundedText> editError;
        // 逻辑布局与 DrawData 同轮封存. 像素请求在编辑应用后生成, 供准备时核对输出.
        Detail::ViewportLayout viewportLayout;
        std::optional<Renderer::ViewportRequest> viewportRequest;
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
        state.edits.emplace();
        if (!state.edits->Id().IsValid())
        {
            return Core::BoundedText("Editor edit session identity exhausted");
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
            // 每个会话只为主 View 注册一次逻辑 token. 随后的布局和换代沿用该身份.
            if (renderer->mainView.IsValid())
            {
                auto token = state.renderer->RegisterView(renderer->mainView);
                if (const auto *error = std::get_if<Core::BoundedText>(&token))
                {
                    return *error;
                }
                state.viewport = Detail::Viewport(renderer->mainView, std::get<UiTextureId>(token));
            }
        }
        else if (!ImGui::GetIO().Fonts->Build())
        {
            return Core::BoundedText("Editor CPU font atlas initialization failed");
        }
        return session;
    }

    // 结束前一轮 UI 借用, 在当前窗口输入边界生成布局, 编辑和宿主值请求.
    EditorFrame EditorSession::Advance(const Platform::HostFrameInput &input, Renderer::View *view,
                                        const Platform::WindowSettings *window)
    {
        auto &state = *m_State;
        if (state.renderer)
        {
            // ImGui 可在下一次 NewFrame 覆写状态或释放旧 atlas 元数据.
            state.renderer->CancelPending();
        }
        state.drawData = nullptr;
        state.viewportLayout = {};
        state.viewportRequest.reset();
        auto &history = *state.edits;
        const auto edits = history.Snapshot();
        if (view && view->Id() != state.viewport.Id())
        {
            state.editError.emplace("Editor frame received a View outside its session");
            view = nullptr;
        }
        const bool renderable = input.window.Renderable();
        const bool inputAvailable = renderable && input.window.focused;
        // 使用宿主发布的窗口末值. 业务按键和失效边沿均从本会话接入后的事件中解释.
        const auto events = state.platform.EventsSinceAttach(input);
        // 即使发布末值已恢复, 批次中的失焦, 最小化或零像素边沿仍会使本批失效.
        // 前一批已停用时, 恢复批次仍先清理队列, 后续批次才重新派发动作.
        const bool discard = !inputAvailable || !state.inputAvailable ||
                             std::ranges::any_of(events, Detail::InterruptsInput);
        EditorFrame frame;
        Detail::WorkspaceFrame workspace;
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
            const auto settings = view ? view->Settings() : Renderer::ViewSettings{};
            workspace = state.workspace.Draw(state.selection, state.viewport, edits, settings, window, state.editError,
                                             !discard);
            if (!discard)
            {
                // 布局可释放弹出层或活动项. 已被占用的本批键盘输入不能随后触发工作区动作.
                const bool activeItem = ImGui::IsAnyItemActive();
                const Detail::InputClaims claims{
                    .blockingPopup = ImGui::IsPopupOpen(nullptr, allPopups),
                    .uiKeyboard = keyboardOwnedBeforeLayout || activeItem,
                    .uiPointer = activeItem ||
                                 (!workspace.viewport.hit && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)),
                    .cursorMode = input.state.cursorMode,
                    .viewportHit = workspace.viewport.hit};
                state.workspace.ApplyShortcuts(events, claims, workspace);
            }
            // 先封存逻辑布局, 再应用事务. 准备期只消费应用后的比例及同批窗口度量.
            ImGui::Render();
            state.drawData = ImGui::GetDrawData();
            state.extentPixels = input.window.framebufferSizePixels;
            state.viewportLayout = workspace.viewport;
            frame.hasDrawData = state.drawData != nullptr;
        }
        // 暂停时也消费事务取消, 由活动 View 尝试还原拖拽起点.
        workspace.scale.cancelled |= discard;
        auto editError = history.Apply(history.Id(), view, workspace.scale, workspace.edits);
        if (editError || workspace.scale.preview || !workspace.edits.empty())
        {
            state.editError = std::move(editError);
        }
        // 用匹配 View 的最新比例和请求代配合封存布局生成像素范围.
        // 隐藏在封存后已生效, 重开要等下一份可见布局才有新的图像矩形.
        if (view && history.Snapshot().viewportVisible)
        {
            state.viewportRequest = state.viewport.Request(state.viewportLayout, input.window, view->Settings());
            frame.viewport = state.viewportRequest;
        }
        frame.titleRequest = std::move(workspace.title);
        frame.vsyncRequest = workspace.vsync;
        // 普通模式的恢复批次可在本轮结束时建立基线. 相对模式保持门控, 等宿主确认归还.
        if (discard && inputAvailable && !frame.hostRequest)
        {
            state.platform.ResumeInput();
        }
        state.inputAvailable = inputAvailable;
        state.suspended = !renderable;
        return frame;
    }

    PrepareUiResult EditorSession::PrepareUiFrame(RHI::Framebuffer *target, Platform::WindowExtent extentPixels,
                                                   Core::FrameArena &arena, const Renderer::NoSceneOutputs &outputs,
                                                   const RHI::CommandListParameters &commands)
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
        const auto *diagnostic = outputs.Diagnostic();
        const auto &layout = state.viewportLayout;
        if (diagnostic && (!state.viewportRequest || diagnostic->view != state.viewportRequest->view ||
                           diagnostic->extent != state.viewportRequest->extent ||
                           outputs.RequestRevision() != state.viewportRequest->requestRevision))
        {
            // 请求与输出不匹配时仅向合成器提供空的 View 输出, 该 token 进入本帧占位解析.
            return state.renderer->Prepare(*state.drawData, target, extentPixels, arena, {}, layout.image, commands);
        }
        return state.renderer->Prepare(*state.drawData, target, extentPixels, arena, outputs, layout.image, commands);
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
