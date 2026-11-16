#include "EditorSession.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include <imgui.h>

#include <DeviceHost.hpp>
#include <Renderer/Prepare/ViewPreparation.hpp>

#include "ActionRouter.hpp"
#include "ImGui/GlfwPlatform.hpp"
#include "ImGui/UiRenderer.hpp"
#include "Panels/Workspace.hpp"
#include "SceneDocuments.hpp"

/// @file
/// @brief 将宿主发布推进为一次 Editor 轮次. 先处理输入失效边沿, 再封存布局和 DrawData.
/// 编辑历史在封存后应用字段意图, 视口像素请求采用同批度量与此时的 View 请求.
/// 文档协调器另由 CPU 输入边界推进, 与本轮 UI 准备及 GPU 合成的成功与否分离.

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
        std::optional<FrameDiagnostic> frameDiagnostic;
        // 逻辑布局与 DrawData 同轮封存. 像素请求在编辑应用后生成, 供准备时核对输出.
        Detail::ViewportLayout viewportLayout;
        std::optional<Renderer::ViewportRequest> viewportRequest;
        std::optional<Renderer::ViewImageLayout> preparedViewport;
        std::optional<Scene::EntityHandle> selection;
        std::unique_ptr<Detail::SceneDocuments> documents;
        const ImDrawData *drawData = nullptr; ///< 借用本轮封存数据, 随下一次 Advance 失效.
        Platform::WindowExtent extentPixels; ///< 仅在 drawData 有效时使用, 单位为 framebuffer 像素.
        // 上一轮是否可接收交互, 与本批次的窗口事实合用.
        bool inputAvailable = true;
        // 上一轮是否跳过 UI 帧, 用于恢复时重建时间基线.
        bool suspended = false;

        /// @brief 同时核对文档和主 View 身份, 不让上一文档的意图作用于当前 owner.
        [[nodiscard]] bool MatchesDocument(Scene::SceneId id, const Renderer::View &view) const noexcept
        {
            return documents && documents->MainView() == view.Id() && documents->Active().Id() == id;
        }

        /// @brief 删除, 历史重放或文档替换后清理失效选择, 不按稳定 ID 自动选择重建后的实体.
        void RetireSelection() noexcept
        {
            if (selection && documents && !documents->Active().Get(*selection))
            {
                selection.reset();
            }
        }
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

    std::optional<Core::BoundedText>
    EditorSession::AttachSceneDocument(std::filesystem::path root, std::unique_ptr<Scene::Scene> scene,
                                       const Renderer::View &view)
    {
        auto &state = *m_State;
        if (state.documents || !scene || !root.is_absolute() ||
            (state.viewport.Id().IsValid() && state.viewport.Id() != view.Id()))
        {
            return Core::BoundedText("Cannot attach a scene document to this session and main View");
        }
        state.documents = std::make_unique<Detail::SceneDocuments>(std::move(root), std::move(scene), view);
        if (!state.viewport.Id().IsValid())
        {
            state.viewport = Detail::Viewport(view.Id(), {});
        }
        return std::nullopt;
    }

    const Scene::Scene *EditorSession::ActiveScene() const noexcept
    {
        return m_State->documents ? &m_State->documents->Active() : nullptr;
    }

    std::optional<SceneDocumentStatus> EditorSession::SceneStatus(const Renderer::View &view) const
    {
        const auto &docs = m_State->documents;
        return docs && docs->MainView() == view.Id() ? std::optional(docs->Status(view)) : std::nullopt;
    }

    std::optional<Scene::EntityHandle> EditorSession::Selection() const noexcept
    {
        return m_State->selection;
    }

    bool EditorSession::Select(std::optional<Scene::EntityHandle> entity) noexcept
    {
        const auto *scene = ActiveScene();
        if (entity && (!scene || !scene->Get(*entity)))
        {
            return false;
        }
        m_State->selection = entity;
        return true;
    }

    std::optional<Core::BoundedText> EditorSession::EditScene(Scene::SceneId scene, Renderer::View &view,
                                                             const Scene::Command &command)
    {
        auto &state = *m_State;
        if (!state.MatchesDocument(scene, view) || state.documents->Frozen())
        {
            return Core::BoundedText("Scene edit requires the active, editable document");
        }
        try
        {
            auto result = state.edits->ApplyScene(state.documents->Active(), view, command);
            state.RetireSelection();
            return result;
        }
        catch (const std::exception &error)
        {
            return Core::BoundedText(error.what());
        }
    }

    std::optional<Core::BoundedText> EditorSession::EditMainView(Scene::SceneId scene, Renderer::View &view,
                                                                Renderer::ViewDocumentRequests requests)
    {
        auto &state = *m_State;
        if (!state.MatchesDocument(scene, view) || state.documents->Frozen())
        {
            return Core::BoundedText("Main View edit requires the active, editable document");
        }
        try
        {
            return state.edits->ApplyView(state.documents->Active(), view, std::move(requests));
        }
        catch (const std::exception &error)
        {
            return Core::BoundedText(error.what());
        }
    }

    std::optional<Core::BoundedText> EditorSession::ReplayEdit(Scene::SceneId scene, Renderer::View &view, bool undo)
    {
        auto &state = *m_State;
        // 停机拒绝整个入口. Open 只冻结文档条目, 交给共享历史按实际条目类型判断重放资格.
        if (!state.MatchesDocument(scene, view) || state.documents->IsStopped())
        {
            return Core::BoundedText("History replay requires an active document session and its main View");
        }
        try
        {
            auto &documents = *state.documents;
            const Detail::EditAction action = undo ? Detail::HistoryAction::Undo : Detail::HistoryAction::Redo;
            auto error = state.edits->Apply(state.edits->Id(), &view, {}, std::span(&action, 1),
                                            &documents.Active(), documents.Frozen());
            state.RetireSelection();
            return error;
        }
        catch (const std::exception &error)
        {
            return Core::BoundedText(error.what());
        }
    }

    std::optional<Core::BoundedText> EditorSession::RequestSceneFile(const SceneFileRequest &request,
                                                                    Renderer::View &view)
    {
        auto &state = *m_State;
        if (!state.documents)
        {
            return Core::BoundedText("No scene document is attached");
        }
        auto result = state.documents->Begin(request, view, *state.edits);
        state.RetireSelection();
        return result;
    }

    std::optional<Core::BoundedText> EditorSession::CancelSceneFile(std::uint64_t operation) noexcept
    {
        return m_State->documents ? m_State->documents->Cancel(operation) :
            std::optional(Core::BoundedText("No scene document is attached"));
    }

    void EditorSession::AdvanceSceneDocument(Asset::AssetManager &assets, Renderer::View &view)
    {
        auto &state = *m_State;
        if (state.documents)
        {
            state.documents->Advance(assets, view, *state.edits);
            state.RetireSelection();
        }
    }

    void EditorSession::ShutdownSceneDocument() noexcept
    {
        if (m_State->documents)
        {
            m_State->documents->Shutdown();
        }
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
        state.preparedViewport.reset();
        auto &history = *state.edits;
        const bool frozen = state.documents && state.documents->Frozen();
        const auto edits = history.Snapshot(frozen);
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
            const auto *acceptedPreview = view ? view->AcceptedScenePreview() : nullptr;
            const auto document = view ? SceneStatus(*view) : std::nullopt;
            workspace = state.workspace.Draw(state.selection, state.viewport, edits, settings, acceptedPreview,
                                             window, state.editError,
                                             state.frameDiagnostic, !discard, document, frozen);
            if (!discard)
            {
                // 布局可释放弹出层或活动项. 已被占用的本批键盘输入不能随后触发工作区动作.
                const bool activeItem = ImGui::IsAnyItemActive();
                const Detail::InputClaims claims{
                    .blockingPopup = ImGui::IsPopupOpen(nullptr, allPopups),
                    .uiKeyboard = keyboardOwnedBeforeLayout || activeItem,
                    .uiPointer = activeItem ||
                                 (!workspace.viewport.panelHovered &&
                                  ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)),
                    .cursorMode = input.state.cursorMode,
                    .viewportHit = workspace.viewport.panelHovered};
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
        std::optional<Core::BoundedText> editError;
        try
        {
            if (workspace.cancelDocument)
            {
                editError = CancelSceneFile(*workspace.cancelDocument);
            }
            if (!editError)
            {
                editError = history.Apply(history.Id(), view, workspace.scale, workspace.edits,
                                          state.documents ? &state.documents->Active() : nullptr, frozen);
            }
        }
        catch (const std::exception &error)
        {
            editError.emplace(error.what());
        }
        state.RetireSelection();
        if (view && workspace.preview)
        {
            // 预览不是文档事务, 仅决定本轮随后的 Forward 准备; 像素生效仍等待 Runtime 接受回执.
            view->RequestPreview(*workspace.preview);
        }
        if (editError || workspace.scale.preview || !workspace.edits.empty() || workspace.cancelDocument)
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

    void EditorSession::SetPreparedViewport(const Renderer::PreparedView *prepared) noexcept
    {
        auto &state = *m_State;
        const auto &request = state.viewportRequest;
        state.preparedViewport.reset();
        if (prepared && prepared->reason == Renderer::ViewSettingReason::None && request &&
            prepared->view == request->view && prepared->requestRevision == request->requestRevision &&
            prepared->layout.outputExtent == request->extent)
        {
            // 只保存与本轮逻辑布局对应的像素映射, 相机和 SceneFrame 仍留在渲染准备结果中.
            state.preparedViewport = prepared->layout;
        }
    }

    std::optional<std::array<double, 2>> EditorSession::ViewportImageUv(double x, double y) const noexcept
    {
        const auto &state = *m_State;
        if (!state.drawData || !state.preparedViewport)
        {
            return std::nullopt;
        }
        return state.viewportLayout.image.ImageUv(*state.drawData, *state.preparedViewport, x, y);
    }

    // 正常模式核对当前布局候选, 安全模式核对已接受快照的来源, 两者共用本轮封存的 UI.
    // 不重放 Advance 或编辑事务, 不在会话状态中保存图版本或取得原生目标.
    PrepareUiResult EditorSession::PrepareUiFrame(Platform::WindowExtent extentPixels,
                                                   Core::FrameArena &arena, const Renderer::PreparedViewOutput &outputs,
                                                   UiFrameMode mode)
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
        const auto *diagnostic = outputs.Color();
        const auto &layout = state.viewportLayout;
        if (!diagnostic || mode != UiFrameMode::Normal)
        {
            // 占位与只读旧颜色都没有本轮图像输入资格.
            state.preparedViewport.reset();
        }
        if (mode == UiFrameMode::SafeUi)
        {
            // 旧输出无需匹配新尺寸和请求代, 但必须来自已接受快照和本会话的 View/角色.
            if (!outputs.IsRetained() || (diagnostic &&
                (diagnostic->view != state.viewport.Id() ||
                 diagnostic->role != Renderer::ViewOutputRole::DisplayColor || !diagnostic->texture)))
            {
                return Core::BoundedText("Safe UI requires this View's retained output snapshot");
            }
            return state.renderer->Prepare(*state.drawData, extentPixels, arena, outputs, layout.image, mode);
        }
        if (outputs.IsRetained() || (diagnostic &&
            (!state.viewportRequest || !state.preparedViewport || diagnostic->view != state.viewportRequest->view ||
             diagnostic->layout != *state.preparedViewport ||
             outputs.RequestRevision() != state.viewportRequest->requestRevision)))
        {
            // 请求与输出不匹配时仅向合成器提供空的 View 输出, 该 token 进入本帧占位解析.
            state.preparedViewport.reset();
            return state.renderer->Prepare(*state.drawData, extentPixels, arena, {}, layout.image, mode);
        }
        return state.renderer->Prepare(*state.drawData, extentPixels, arena, outputs, layout.image, mode);
    }

    bool EditorSession::BaseFontReady() const noexcept
    {
        return m_State->renderer && m_State->renderer->BaseFontReady();
    }

    // 一份最近失败覆盖旧失败. 正常恢复仅改变恢复状态, 保留原文供维护者回看.
    void EditorSession::ReportFrameOutcome(FrameRecovery recovery, const std::optional<Core::BoundedText> &primary,
                                            const std::optional<Core::BoundedText> &additional) noexcept
    {
        if (primary)
        {
            m_State->frameDiagnostic = FrameDiagnostic{*primary, additional, recovery};
        }
        else if (m_State->frameDiagnostic)
        {
            m_State->frameDiagnostic->recovery = recovery;
        }
    }

    UiTextureResult EditorSession::RegisterUiTexture(RHI::Texture &texture, UiTextureImport imported)
    {
        auto &renderer = m_State->renderer;
        if (!renderer)
        {
            return Core::BoundedText("Editor UI texture registration requires an attached renderer");
        }
        return renderer->RegisterResident(texture, imported);
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
