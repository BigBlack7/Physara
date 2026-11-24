#include "EditorSession.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <exception>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include <DeviceHost.hpp>
#include <Renderer/Prepare/ViewPreparation.hpp>

#include "ActionRouter.hpp"
#include "ImGui/GlfwPlatform.hpp"
#include "ImGui/UiRenderer.hpp"
#include "Panels/Workspace.hpp"
#include "SceneDocuments.hpp"
#include "SceneImport.hpp"
#include "Viewport/ViewportNavigation.hpp"

/// @file
/// @brief 将宿主发布推进为一次 Editor 轮次. 先处理输入失效边沿, 再封存布局和 DrawData.
/// 编辑历史在封存后应用字段意图, 视口像素请求采用同批度量与此时的 View 请求.
/// 物理参数与相机引用共用主 View 事务, 由该入口核对文档冻结, 目标身份和历史资格.
/// 文档协调器另由 CPU 输入边界推进, 与本轮 UI 准备及 GPU 合成的成功与否分离.

namespace Physara::Editor
{
    /// @brief 聚合本会话的布局, 编辑与纹理身份, 以及一轮内有效的 UI 借用.
    struct EditorSession::State
    {
        /// @brief 本次正常合成期望的布局与场景来源, 从 Advance 后的准备值按值封存.
        struct PreparedViewport
        {
            Renderer::ViewLayouts layouts;
            Scene::SceneId scene;
            std::uint64_t contentRevision;
            std::uint64_t sceneCameraRevision; ///< 与所属 Scene 一起匹配; View 的相机请求由文档代覆盖.
            std::uint64_t documentRevision;
            std::uint64_t shadowRequestRevision;
            Renderer::PreviewMode preview;

            /// @pre prepared 已成功准备相机及所属场景.
            explicit PreparedViewport(const Renderer::PreparedView &prepared) noexcept
                : layouts(prepared.layouts), scene(prepared.scene.scene), contentRevision(prepared.scene.contentRevision),
                  sceneCameraRevision(prepared.camera->sceneCameraRevision), documentRevision(prepared.documentRevision),
                  shadowRequestRevision(prepared.shadowStatistics.requestRevision), preview(prepared.preview)
            {
            }

            bool operator==(const PreparedViewport &) const noexcept = default;

            /// @brief 测试图核对渲染布局, 场景色还核对两套布局及生产来源. 调用方核对 View 身份和比例请求代.
            /// 选中灯无可用阴影组时也可使用本轮普通场景色, 其阴影请求代仍须与当前准备值相同.
            [[nodiscard]] bool Matches(const Renderer::ViewOutput &output) const noexcept
            {
                const auto &rendered = output.scene;
                if (!rendered)
                {
                    return output.layout == layouts.render;
                }
                return output.layout == layouts.display && rendered->renderLayout == layouts.render &&
                       rendered->scene == scene && rendered->contentRevision == contentRevision &&
                       rendered->camera.sceneCameraRevision == sceneCameraRevision &&
                       rendered->documentRevision == documentRevision && rendered->mode == preview &&
                       rendered->shadowRequestRevision == shadowRequestRevision;
            }
        };

        /// @brief 正常图已采样并确认的坐标依据. 两种视口颜色都带当时的 Scene 和相机版本.
        struct AcceptedImage
        {
            Renderer::ViewId view;
            Scene::SceneId scene;
            std::uint64_t contentRevision;
            std::uint64_t sceneCameraRevision;
            std::uint64_t viewCameraRevision;
            std::uint64_t requestRevision;
            std::uint64_t metricsRevision;
            Renderer::PreviewMode preview;
            Renderer::AntiAliasing requestedAa; ///< 命中匹配当时的请求, 不是诊断场景固定采用的 None.
            Detail::ViewportRegion region;
            Renderer::ViewImageLayout layout;
        };

        // 平台适配最后析构, renderer 先结束借用并解除纹理关联, 再销毁 UI context.
        Detail::GlfwPlatform platform;
        std::unique_ptr<Detail::UiRenderer> renderer;
        Detail::Workspace workspace;
        Detail::Viewport viewport;
        Detail::ViewportNavigation navigation;
        // 编辑状态在平台适配接入成功后建立, 结束会话时释放事务与历史.
        std::optional<Detail::EditHistory> edits;
        std::optional<Core::BoundedText> editError;
        std::optional<Core::BoundedText> fileError;
        std::optional<FrameDiagnostic> frameDiagnostic;
        // 逻辑布局与 DrawData 同轮封存. 像素请求在编辑应用后生成, 供准备时核对输出.
        Detail::ViewportLayout viewportLayout;
        std::optional<Detail::ViewportRegion> viewportRegion;
        std::optional<Renderer::ViewportRequest> viewportRequest;
        std::optional<PreparedViewport> preparedViewport; ///< 候选专用, 仅供本轮 UI 准备校验.
        std::optional<AcceptedImage> acceptedImage;
        std::optional<Renderer::ViewImageLayout> imageInput; ///< 本轮匹配后的命中映射, 不代表图候选已被接受.
        std::uint64_t metricsRevision{};
        std::optional<Scene::EntityHandle> selection;
        std::unique_ptr<Detail::SceneDocuments> documents;
        Detail::SceneImport modelImport;
        Detail::PhysicalEdits physicalEdits;
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

        /// @brief 只有当前画布、文档及请求仍匹配时才借用已接受图像的矩形.
        [[nodiscard]] std::optional<Renderer::ViewImageLayout> MatchImage(const Scene::Scene *scene,
                                                                         const Renderer::View *view) const noexcept
        {
            if (!acceptedImage || !scene || !view || viewportRegion != acceptedImage->region ||
                metricsRevision != acceptedImage->metricsRevision || !edits->Snapshot().viewportVisible)
            {
                return std::nullopt;
            }
            const auto &image = *acceptedImage;
            const auto revisions = scene->Versions();
            const auto settings = view->Settings();
            if (view->Id() != image.view || scene->Id() != image.scene || revisions.renderContent != image.contentRevision ||
                revisions.camera != image.sceneCameraRevision || view->CameraRevision() != image.viewCameraRevision ||
                settings.requestRevision != image.requestRevision || settings.preview != image.preview ||
                settings.requestedAa != image.requestedAa)
            {
                return std::nullopt;
            }
            return image.layout;
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

    void EditorSession::AcknowledgeSceneRenderChanges() noexcept
    {
        assert(m_State->documents);
        m_State->documents->Active().AcknowledgeRenderChanges();
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
            auto result = state.physicalEdits.ApplyScene(state.documents->Active(), view, *state.edits, command);
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
            auto error = state.edits->Apply(state.edits->Id(), &view, std::span(&action, 1),
                                            &documents.Active(), documents.Frozen());
            state.physicalEdits.RefreshSourceStatus(documents.Active());
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
        if (!result && (request.action == SceneFileAction::New || request.action == SceneFileAction::Open))
        {
            state.modelImport.Retire();
            state.physicalEdits.Reset();
        }
        state.RetireSelection();
        return result;
    }

    std::optional<Core::BoundedText> EditorSession::SaveBeforeReplacing(SceneFileRequest replacement,
        const std::filesystem::path &saveAs, Renderer::View &view)
    {
        auto &state = *m_State;
        if (!state.documents)
        {
            return Core::BoundedText("No scene document is attached");
        }
        return state.documents->SaveBeforeReplacing(std::move(replacement), saveAs, view, *state.edits);
    }

    std::optional<Core::BoundedText> EditorSession::CancelSceneFile(std::uint64_t operation) noexcept
    {
        return m_State->documents ? m_State->documents->Cancel(operation) :
            std::optional(Core::BoundedText("No scene document is attached"));
    }

    std::optional<Core::BoundedText> EditorSession::RequestModelImport(Asset::AssetManager &assets,
        Scene::SceneId scene, const std::filesystem::path &path, std::optional<std::uint32_t> sourceScene)
    {
        auto &state = *m_State;
        if (!state.documents || state.documents->Active().Id() != scene || state.documents->Frozen())
        {
            return Core::BoundedText("Model import requires the active, editable document");
        }
        return state.modelImport.Begin(assets, scene, path, {sourceScene});
    }

    const SceneImportStatus &EditorSession::ImportStatus() const noexcept { return m_State->modelImport.Status(); }

    bool EditorSession::CancelModelImport(std::uint64_t operation) noexcept
    {
        auto &model = m_State->modelImport;
        if (!model.Status().Pending() || model.Status().operation != operation)
        {
            return false;
        }
        model.Retire();
        return true;
    }

    // 文件结果可能替换整个 Scene. 先处理该边界, 再让导入按新活动身份决定提交或退役.
    void EditorSession::AdvanceSceneDocument(Asset::AssetManager &assets, Renderer::View &view)
    {
        auto &state = *m_State;
        if (state.documents)
        {
            const auto before = state.documents->Active().Id();
            state.documents->Advance(assets, view, *state.edits);
            state.RetireSelection();
            if (state.documents->Active().Id() != before || state.documents->Frozen())
            {
                state.physicalEdits.Reset();
            }
            if (state.documents->Frozen())
            {
                state.modelImport.Retire();
            }
            else if (state.documents->MainView() == view.Id())
            {
                state.modelImport.Advance(assets, state.documents->Active(), view, *state.edits);
                state.physicalEdits.Advance(assets, state.documents->Active(), view, *state.edits);
            }
        }
    }

    void EditorSession::ShutdownSceneDocument() noexcept
    {
        m_State->modelImport.Retire();
        m_State->physicalEdits.Reset();
        m_State->navigation.Reset();
        if (m_State->documents)
        {
            m_State->documents->Shutdown();
        }
    }

    // UTF-8 草稿在提交边界转成路径. 路径范围和物理落点继续由文件与资产入口校验.
    static std::filesystem::path DraftPath(const std::string &text)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
    }

    // 文件和实例意图各走自己的接受入口, 取消使用 UI 观察到的操作编号.
    static std::optional<Core::BoundedText> ApplyFileIntents(EditorSession &editor,
        const Detail::WorkspaceFrame &workspace, Renderer::View &view, Asset::AssetManager *assets)
    {
        if (workspace.cancelDocument)
        {
            return editor.CancelSceneFile(*workspace.cancelDocument);
        }
        if (workspace.cancelImport && !editor.CancelModelImport(*workspace.cancelImport))
        {
            return Core::BoundedText("Model import is no longer active");
        }
        if (const auto &intent = workspace.file)
        {
            SceneFileRequest request{intent->action, intent->version.scene, DraftPath(intent->path), {}};
            if (intent->decision == Detail::ReplaceDecision::Save)
            {
                return editor.SaveBeforeReplacing(std::move(request), DraftPath(intent->savePath), view);
            }
            if (intent->decision == Detail::ReplaceDecision::Discard)
            {
                request.discard = intent->version;
            }
            return editor.RequestSceneFile(request, view);
        }
        if (const auto &intent = workspace.import)
        {
            if (!assets)
            {
                return Core::BoundedText("Model import requires the project asset service");
            }
            std::optional<std::uint32_t> scene;
            if (!intent->sourceScene.empty())
            {
                const auto *end = intent->sourceScene.data() + intent->sourceScene.size();
                std::uint32_t value{};
                const auto parsed = std::from_chars(intent->sourceScene.data(), end, value);
                if (parsed.ec != std::errc{} || parsed.ptr != end)
                {
                    return Core::BoundedText("Source scene must be a nonnegative integer index");
                }
                scene = value;
            }
            return editor.RequestModelImport(*assets, intent->scene, DraftPath(intent->path), scene);
        }
        return std::nullopt;
    }

    // Escape 由当前手势消费. 失去资格时用相同取消路径恢复首值.
    static bool IsEscape(const Platform::InputEvent &event) noexcept
    {
        const auto *key = std::get_if<Platform::KeyEvent>(&event);
        return key && key->key == Platform::Key::Escape && key->action == Platform::KeyAction::Press;
    }

    // 同批已经释放的 Press 在宿主收到捕获请求前便已结束.
    static bool StillHeld(const Platform::InputSnapshot &input, Platform::MouseButton button,
                         std::span<const Platform::InputEvent> after) noexcept
    {
        const auto state = input.MouseState(button);
        return state.down && (!state.released || std::ranges::none_of(after, [button](const auto &event)
        {
            const auto *edge = std::get_if<Platform::MouseButtonEvent>(&event);
            return edge && edge->button == button && edge->action == Platform::KeyAction::Release;
        }));
    }

    // 先封存布局, 再按本轮输入和领域状态产生事务及像素请求.
    EditorFrame EditorSession::Advance(const Platform::HostFrameInput &input, Renderer::View *view,
        const Platform::WindowSettings *window, Asset::AssetManager *assets, const FrameStatistics *statistics)
    {
        auto &state = *m_State;
        if (state.renderer)
        {
            state.renderer->CancelPending();
        }
        state.drawData = nullptr;
        state.viewportLayout = {};
        state.viewportRegion.reset();
        state.viewportRequest.reset();
        state.preparedViewport.reset();
        state.imageInput.reset();
        state.metricsRevision = input.window.metricsRevision;
        auto &history = *state.edits;
        auto *scene = state.documents ? &state.documents->Active() : nullptr;
        const bool frozen = state.documents && state.documents->Frozen();
        if (view && view->Id() != state.viewport.Id())
        {
            state.editError.emplace("Editor frame received a View outside its session");
            view = nullptr;
        }
        const auto events = state.platform.EventsSinceAttach(input);
        const bool renderable = input.window.Renderable();
        const bool inputAvailable = renderable && input.window.focused;
        // 恢复批次及含中间失效边沿的批次只清理交互, 下一批才允许新动作.
        const bool discard = !inputAvailable || !state.inputAvailable ||
                             std::ranges::any_of(events, Detail::InterruptsInput);
        const bool escapePressed = !discard && std::ranges::any_of(events, IsEscape);
        const bool wasNavigating = state.navigation.Active();
        EditorFrame frame;
        Detail::WorkspaceFrame workspace;
        auto document = view ? SceneStatus(*view) : std::nullopt;
        Detail::WorkspaceState snapshot{scene, state.selection, view, history.Snapshot(frozen), window,
            document ? &*document : nullptr, state.modelImport.Status(), state.physicalEdits.Status(),
            state.editError, state.fileError,
            state.frameDiagnostic, statistics, !discard, frozen, wasNavigating};
        Detail::InputClaims claims{.cursorMode = input.state.cursorMode};
        if (discard)
        {
            state.platform.DiscardInput(input.window.focused);
        }
        if (renderable)
        {
            state.platform.BeginFrame(input.window, state.suspended);
            constexpr auto allPopups = ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel;
            const bool popupBefore = ImGui::IsPopupOpen(nullptr, allPopups);
            // 保留布局前的占用, 防止文本或弹窗在本帧释放后, 同批按键又触发领域快捷键.
            const bool keyboardBefore = popupBefore || ImGui::IsAnyItemActive() || ImGui::GetIO().WantTextInput;
            workspace = state.workspace.Draw(snapshot, state.viewport);
            claims.blockingPopup = popupBefore || ImGui::IsPopupOpen(nullptr, allPopups);
            const bool active = ImGui::IsAnyItemActive();
            claims.uiKeyboard = keyboardBefore || active || ImGui::GetIO().WantTextInput;
            claims.uiPointer = active || ImGui::GetCurrentContext()->MovingWindow != nullptr;
            ImGui::Render();
            state.drawData = ImGui::GetDrawData();
            state.extentPixels = input.window.framebufferSizePixels;
            state.viewportLayout = workspace.viewport;
            frame.hasDrawData = state.drawData != nullptr;
            if (state.drawData)
            {
                state.viewportRegion = workspace.viewport.image.Region(*state.drawData);
            }
        }

        try
        {
            if (wasNavigating)
            {
                // 捕获中的手势可等待自己的相机回执, 但失焦, 隐藏和更高优先级 UI 会取消原预览.
                const bool hiding = std::ranges::any_of(workspace.edits, [](const auto &action)
                {
                    const auto *visibility = std::get_if<Detail::SetViewportVisibility>(&action);
                    return visibility && !visibility->visible;
                });
                const bool eligible = !discard && !frozen && !hiding && !claims.blockingPopup &&
                                      !claims.uiKeyboard && !claims.uiPointer && !claims.gizmoActive;
                workspace.gestures.current = state.navigation.Step(scene, view, state.viewportRegion, input, events,
                    eligible, ImGui::GetIO().DeltaTime);
            }
            else if (discard || frozen || claims.blockingPopup || escapePressed)
            {
                if (history.HasGesture())
                {
                    ImGui::ClearActiveID();
                    state.workspace.EndRotationInput();
                }
                workspace.gestures = {};
            }

            // 完成旧项后才开启新项. 失效时输入为空, 沿原字段协议恢复已接受的预览.
            const auto &gestures = workspace.gestures;
            if (history.HasGesture() || gestures.completed || gestures.current.began)
            {
                auto result = history.ApplyGestures(scene, view, gestures);
                if (result || gestures.current.preview || (gestures.completed && gestures.completed->preview))
                {
                    state.editError = std::move(result);
                }
            }
            const auto apply = [&](std::span<const Detail::EditAction> actions)
            {
                if (!actions.empty())
                {
                    state.editError = history.Apply(history.Id(), view, actions, scene, frozen);
                    if (scene)
                    {
                        state.physicalEdits.RefreshSourceStatus(*scene);
                    }
                    state.RetireSelection();
                    document = view ? SceneStatus(*view) : std::nullopt;
                    snapshot.document = document ? &*document : nullptr;
                    snapshot.selection = state.selection;
                    snapshot.edits = history.Snapshot(frozen);
                }
            };
            // 领域修改先落到当前 owner, 选择再按存活 handle 接受; 后续文件操作可能替换整个文档.
            apply(workspace.edits);
            if (workspace.selection)
            {
                static_cast<void>(Select(workspace.selection));
                snapshot.selection = state.selection;
            }
            if (view && scene && workspace.camera)
            {
                auto requests = view->DocumentRequests();
                requests.cameraEntityId = std::move(workspace.camera->entity);
                state.editError = EditMainView(scene->Id(), *view, std::move(requests));
            }
            if (view && scene && workspace.physicalCamera)
            {
                // 只把提交的字段合入当前请求, 保留本批先前相机或历史动作的结果.
                // 数值已在面板换成领域单位, EditMainView 仍核对冻结状态并经 View 预检后记录撤销项.
                auto requests = view->DocumentRequests();
                const auto &edit = *workspace.physicalCamera;
                requests.physicalCamera.*edit.field = edit.value;
                state.editError = EditMainView(scene->Id(), *view, std::move(requests));
            }
            if (view && workspace.preview)
            {
                view->RequestPreview(*workspace.preview);
            }
            if (view && workspace.shadowPlanning && !frozen && !discard)
            {
                // 控件只提交开关. 在帧边界取该 View 当前的距离与档位一起校验, 会话设置无需写入撤销历史.
                auto settings = view->RequestedShadows();
                settings.enabled = workspace.shadowPlanning->enabled;
                if (auto error = view->RequestShadows(workspace.shadowPlanning->view, settings))
                {
                    state.editError = std::move(error);
                }
            }
            if (view && scene && workspace.shadowDiagnostic && !frozen && !discard)
            {
                const auto &request = *workspace.shadowDiagnostic;
                if (auto error = view->RequestShadowDiagnostic(request.view, *scene, request.selection))
                {
                    state.editError = std::move(error);
                }
            }
            if (view && scene && workspace.antiAliasing)
            {
                auto requests = view->DocumentRequests();
                requests.antiAliasing = *workspace.antiAliasing;
                state.editError = EditMainView(scene->Id(), *view, std::move(requests));
            }
            if (view && scene && !frozen && !discard)
            {
                // 控件可以来自同一批布局. 按顺序把每个字段合入当前 Scene, 文件替换在本批最后处理.
                if (workspace.cancelPhysicalSource)
                {
                    state.physicalEdits.Cancel();
                }
                if (workspace.physicalInputError || !workspace.physicalEdits.empty())
                {
                    // 文本解析失败没有对应的领域命令. 保留本批首错, 同批其它字段仍逐项提交.
                    state.editError = std::move(workspace.physicalInputError);
                    for (const auto &edit : workspace.physicalEdits)
                    {
                        if (const auto error = state.physicalEdits.Apply(*scene, *view, history, edit);
                            error && !state.editError)
                        {
                            state.editError = error;
                        }
                    }
                }
                if (workspace.physicalSource)
                {
                    if (assets)
                    {
                        state.physicalEdits.Begin(*assets, *scene, *workspace.physicalSource);
                    }
                    else
                    {
                        state.editError.emplace("No asset service is attached to this editor frame");
                    }
                }
            }

            if (!discard && !wasNavigating)
            {
                for (std::size_t i = 0; i < events.size(); ++i)
                {
                    // 先到的文件动作获得模态或提交机会, 本批不再打开另一个文件动作或相机捕获.
                    if (workspace.file || workspace.import || state.workspace.HasFileDialog())
                    {
                        break;
                    }
                    if (std::holds_alternative<Platform::KeyEvent>(events[i]))
                    {
                        const auto count = workspace.edits.size();
                        state.workspace.ApplyShortcut(events[i], claims, snapshot, workspace);
                        apply(std::span<const Detail::EditAction>(workspace.edits).subspan(count));
                        continue;
                    }
                    const auto *press = std::get_if<Platform::MouseButtonEvent>(&events[i]);
                    if (!press || press->action != Platform::KeyAction::Press || !scene || !view || frozen ||
                        escapePressed ||
                        history.HasGesture() || view->DocumentRequests().cameraEntityId || claims.uiKeyboard ||
                        input.state.cursorMode != Platform::CursorMode::Normal)
                    {
                        continue;
                    }
                    // 新捕获必须同时命中当前正常图像与顶层窗口, 不能只看事件批次末的指针位置.
                    const auto layout = state.MatchImage(scene, view);
                    const auto cursor = press->positionLogical;
                    if (!layout || !state.viewportRegion->ImageUv(*layout, cursor.x, cursor.y))
                    {
                        continue;
                    }
                    claims.viewportHit = state.viewportLayout.OwnsPointer(cursor);
                    if (Detail::ResolveInputOwner(claims, events[i]) != Detail::InputOwner::Viewport ||
                        !StillHeld(input.state, press->button, events.subspan(i + 1)))
                    {
                        continue;
                    }
                    const auto start = state.navigation.Start(*scene, *view, state.selection,
                        *state.viewportRegion, input.window, *press);
                    if (start.began)
                    {
                        if (auto error = history.ApplyGesture(scene, view, start))
                        {
                            state.navigation.Reset();
                            state.editError = std::move(error);
                        }
                        else
                        {
                                    auto &gui = *ImGui::GetCurrentContext();
                                    // ImGui 在封存末尾可能把图片上的左键当作窗口背景拖动.
                                    // 本轮布局时没有活动控件, 此次按下已由 Orbit 接收.
                                    if (press->button == Platform::MouseButton::Primary && gui.ActiveIdWindow &&
                                        gui.ActiveId == gui.ActiveIdWindow->MoveId)
                                    {
                                        ImGui::StopMouseMovingWindow();
                                        ImGui::ClearActiveID();
                                    }
                            frame.hostRequest = Platform::SetCursorMode{Platform::CursorMode::Relative};
                        }
                        break;
                    }
                }
            }
            // New 可同步替换 Scene. 此后不再使用本轮开头借出的 scene, 最终映射重新读取活动文档.
            if (view && (workspace.file || workspace.import || workspace.cancelDocument || workspace.cancelImport))
            {
                try
                {
                    state.fileError = ApplyFileIntents(*this, workspace, *view, assets);
                }
                catch (const std::filesystem::filesystem_error &error)
                {
                    state.fileError.emplace(error.what());
                }
            }
        }
        catch (const std::exception &error)
        {
            state.editError.emplace(error.what());
        }

        if (!state.navigation.Active() && input.state.cursorMode == Platform::CursorMode::Relative)
        {
            frame.hostRequest = Platform::SetCursorMode{Platform::CursorMode::Normal};
        }
        // 使用当轮最终比例及显隐生成像素请求, 与是否正在捕获相机输入分开处理.
        if (view && history.Snapshot().viewportVisible)
        {
            state.viewportRequest = state.viewport.Request(state.viewportLayout, input.window, view->Settings());
            frame.viewport = state.viewportRequest;
        }
        if (!state.viewportRegion || !frame.viewport)
        {
            state.acceptedImage.reset();
        }
        state.imageInput = state.MatchImage(ActiveScene(), view);
        frame.titleRequest = std::move(workspace.title);
        frame.vsyncRequest = workspace.vsync;
        // 普通模式的恢复批次重建指针基线. 相对模式等 Runtime 完成实际归还.
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
        if (prepared && prepared->reason == Renderer::ViewSettingReason::None && prepared->camera && request &&
            prepared->view == request->view && prepared->requestRevision == request->requestRevision &&
            prepared->layouts.display.outputExtent == request->outputExtent &&
            prepared->layouts.render.outputExtent == request->renderExtent)
        {
            state.preparedViewport.emplace(*prepared);
        }
    }

    // 调用方已核实正常图实际采样, 提交及 View 发布. 此处只复制身份/布局, 不保留帧槽借用.
    void EditorSession::SetAcceptedViewport(const Renderer::PreparedView *accepted, const Renderer::View &view) noexcept
    {
        auto &state = *m_State;
        if (!accepted || !state.viewportRegion)
        {
            state.acceptedImage.reset();
            state.imageInput.reset();
            return;
        }
        assert(state.preparedViewport && state.viewportRequest && accepted->camera &&
               state.viewportRequest->view == accepted->view && state.viewportRequest->requestRevision ==
                   accepted->requestRevision && state.preparedViewport == State::PreparedViewport{*accepted});
        // 同一个 token 可以显示两种画布. 场景色已经过输出适配, 无场景测试图仍使用渲染栅格.
        const auto &layout = view.AcceptedScenePreview() ? accepted->layouts.display : accepted->layouts.render;
        state.acceptedImage = State::AcceptedImage{accepted->view, accepted->scene.scene,
            accepted->scene.contentRevision, accepted->camera->sceneCameraRevision, accepted->cameraRevision,
            accepted->requestRevision, state.metricsRevision, accepted->preview, accepted->requestedAa,
            *state.viewportRegion, layout};
        state.imageInput = layout;
    }

    std::optional<std::array<double, 2>> EditorSession::ViewportImageUv(double x, double y) const noexcept
    {
        const auto &state = *m_State;
        return state.viewportRegion && state.imageInput ? state.viewportRegion->ImageUv(*state.imageInput, x, y) :
            std::nullopt;
    }

    // 正常模式按本轮准备的布局及生产来源筛选场景色. 安全模式另取已接受快照, 不确认当前请求.
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
        const auto *color = outputs.Color();
        const auto &layout = state.viewportLayout;
        if (!color || mode != UiFrameMode::Normal)
        {
            // 占位与只读旧颜色都没有本轮图像输入资格.
            state.imageInput.reset();
        }
        if (mode == UiFrameMode::SafeUi)
        {
            // 旧输出可以沿用旧布局. 除本会话的 View 和颜色角色外, 池对象还须有可接入的录制状态.
            if (!outputs.IsRetained() || (color &&
                (color->view != state.viewport.Id() ||
                 color->role != Renderer::ViewOutputRole::DisplayColor || !color->HasReadableColor())))
            {
                return Core::BoundedText("Safe UI requires this View's retained output snapshot");
            }
            return state.renderer->Prepare(*state.drawData, extentPixels, arena, outputs, layout.image, mode);
        }
        if (outputs.IsRetained() || (color &&
            (!state.viewportRequest || !state.preparedViewport || color->view != state.viewportRequest->view ||
             !state.preparedViewport->Matches(*color) ||
             outputs.RequestRevision() != state.viewportRequest->requestRevision)))
        {
            // 生产来源失配时在图内采样前改用占位并撤销命中, 尺寸相同的旧场景也适用.
            state.imageInput.reset();
            return state.renderer->Prepare(*state.drawData, extentPixels, arena, {}, layout.image, mode);
        }
        return state.renderer->Prepare(*state.drawData, extentPixels, arena, outputs, layout.image, mode);
    }

    bool EditorSession::HasViewportSample(Platform::WindowExtent extentPixels) const noexcept
    {
        const auto &state = *m_State;
        return state.drawData && state.renderer && extentPixels == state.extentPixels &&
            Detail::UiRenderer::HasViewSample(*state.drawData, extentPixels, state.viewportLayout.image.token);
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
