#include "Workspace.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <variant>

#include <imgui.h>
#include <imgui_internal.h>

#include "Session/ActionRouter.hpp"

/// @file
/// @brief 在主窗口绘制停靠面板, 收集手势, 菜单及快捷键动作.
/// 编辑意图交给 EditorSession, 标题和 VSync 请求随本轮结果交给 Runtime.

namespace Physara::Editor::Detail
{
    /// @brief 面板菜单项及固定的 ImGui 窗口身份.
    struct Panel
    {
        ActionId action;
        const char *menu;
        const char *window;
        const char *shortcut;
    };

    // 稳定 ID 保留隐藏前的停靠关系. 首次停靠组选择最后加入的 Viewport 页.
    static constexpr std::array Panels{
        Panel{ActionId::ToggleHierarchy, "Hierarchy", "Hierarchy###Physara.Hierarchy", nullptr},
        Panel{ActionId::ToggleInspector, "Inspector", "Inspector###Physara.Inspector", "Ctrl+Shift+I"},
        Panel{ActionId::ToggleViewSettings, "View Settings", "View Settings###Physara.ViewSettings", nullptr},
        Panel{ActionId::ToggleViewport, "Viewport", "Viewport###Physara.Viewport", nullptr}};

    constexpr auto Control = static_cast<unsigned>(Platform::Modifier::Control);
    constexpr auto Shift = static_cast<unsigned>(Platform::Modifier::Shift);
    constexpr auto ShortcutModifiers = Control | Shift | static_cast<unsigned>(Platform::Modifier::Alt) |
                                       static_cast<unsigned>(Platform::Modifier::Super);

    /// @brief 文档和编辑动作的菜单标签与键盘映射共用同一描述.
    struct CommandItem
    {
        ActionId action;
        const char *label;
        const char *shortcut;
        Platform::Key key;
        unsigned modifiers;
    };

    static constexpr std::array FileItems{
        CommandItem{ActionId::NewScene, "New Scene", "Ctrl+N", Platform::Key::N, Control},
        CommandItem{ActionId::OpenScene, "Open Scene", "Ctrl+O", Platform::Key::O, Control},
        CommandItem{ActionId::SaveScene, "Save Scene", "Ctrl+S", Platform::Key::S, Control},
        CommandItem{ActionId::SaveSceneAs, "Save Scene As", "Ctrl+Shift+S", Platform::Key::S, Control | Shift},
        CommandItem{ActionId::ImportModel, "Import Model", nullptr, Platform::Key::Unknown, 0}};

    static constexpr std::array EditItems{
        CommandItem{ActionId::Undo, "Undo", "Ctrl+Z", Platform::Key::Z, Control},
        CommandItem{ActionId::Redo, "Redo", "Ctrl+Y", Platform::Key::Y, Control},
        CommandItem{ActionId::DeleteSelection, "Delete", "Delete", Platform::Key::Delete, 0}};

    // 将 View 提供的等待和拒绝原因映射为 View Settings 标签.
    static const char *ViewStatus(Renderer::ViewSettingReason reason) noexcept
    {
        using enum Renderer::ViewSettingReason;
        switch (reason)
        {
        case None: return "Applied";
        case LayoutPending: return "Pending - waiting for viewport layout";
        case SlotPending: return "Pending - waiting for a frame slot";
        case SubmissionPending: return "Pending - waiting for accepted output";
        case UploadPending: return "Pending - waiting for upload";
        case InvalidExtent: return "Failed - invalid viewport pixel extent";
        case TextureSizeLimit: return "Failed - device texture size limit";
        case DiagnosticBudget: return "Failed - diagnostic texture budget";
        case CameraUnavailable: return "Failed - scene camera unavailable";
        case InvalidProjection: return "Failed - invalid camera projection";
        case InvalidImageRect: return "Failed - camera image does not fit the viewport";
        case InvalidSampling: return "Failed - invalid camera sampling profile";
        case ScenePreparationFailed: return "Failed - scene preparation";
        case ScenePipelineUnavailable: return "Failed - Forward pipeline unavailable";
        case SceneResidencyPending: return "Pending - waiting for scene residency";
        case SceneResidencyLimit: return "Failed - scene residency budget";
        case SceneAttachmentLimit: return "Failed - Forward attachment limit";
        case SceneUnsupported: return "Failed - unsupported visible surfaces";
        case SceneUploadLimit: return "Failed - Forward upload limit";
        }
        return "";
    }

    // 用权威比例快照初始化本轮控件, 从 ImGui 项状态提取比例意图.
    static void DrawViewSettings(const Renderer::ViewSettings &view, bool allowEdits, WorkspaceFrame &frame)
    {
        constexpr char scaleLabel[] = "Render scale";
        auto scale = view.requestedScale;
        const auto &context = *ImGui::GetCurrentContext();
        const auto slider = ImGui::GetID(scaleLabel);
        const bool sampleRelease = allowEdits && context.ActiveId == slider &&
                                   context.ActiveIdSource == ImGuiInputSource_Mouse &&
                                   ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsMousePosValid();
        auto &mouseDown = ImGui::GetIO().MouseDown[ImGuiMouseButton_Left];
        if (sampleRelease)
        {
            // 固定版 SliderBehavior 在释放时先清除活动项. 暂时保持本滑块的按下状态,
            // 让它用原有抓取偏移和舍入计算末段位置. 真实的释放边沿仍供手势判断.
            mouseDown = true;
        }
        ImGui::BeginDisabled(!allowEdits);
        const bool changed = ImGui::SliderFloat(scaleLabel, &scale, Renderer::MinRenderScale,
                                                Renderer::MaxRenderScale, "%.2fx", ImGuiSliderFlags_NoInput);
        ImGui::EndDisabled();
        if (sampleRelease)
        {
            // 先恢复真实按钮状态并结束活动项, 再读取项状态以提交本次末段采样.
            mouseDown = false;
            if (context.ActiveId == slider)
            {
                ImGui::ClearActiveID();
            }
        }
        if (allowEdits)
        {
            // 释放和导航确认作为正常结束信号, Escape 通过手势的取消标志处理.
            const bool began = ImGui::IsItemActivated();
            const bool active = ImGui::IsItemActive();
            const bool deactivated = ImGui::IsItemDeactivated();
            const bool finished = deactivated && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
            const auto preview = changed ? std::optional<FieldValue>(scale) : std::nullopt;
            if (finished && slider == context.ActiveIdPreviousFrame)
            {
                frame.gestures.completed = FieldGesture{view.view, preview, false, true};
            }
            else if (active || began)
            {
                frame.gestures.current = {view.view, preview, began, false};
            }
            else if (changed && !deactivated)
            {
                frame.edits.emplace_back(SetViewScale{view.view, scale});
            }
        }
        ImGui::Text("Requested %.2fx", view.requestedScale);
        if (view.effective)
        {
            // 显示最后已接受的整张画布尺寸, 不是当前请求或留边后的相机采样尺寸.
            const auto extent = view.effective->layout.outputExtent;
            ImGui::Text("Effective %.2fx (%u x %u)", view.effective->scale,
                        extent.width, extent.height);
        }
        else
        {
            ImGui::TextUnformatted("Effective - awaiting confirmation");
        }
        ImGui::TextUnformatted(ViewStatus(view.reason));
    }

    // 标题正文仍以原始 UTF-8 编辑和提交. 缺字及控制码用可区分的码点显示, 反斜杠转义.
    static std::string DisplayWindowTitle(std::string_view title)
    {
        std::string display;
        display.reserve(title.size());
        auto *font = ImGui::GetFont();
        for (auto it = title.data(), end = it + title.size(); it != end;)
        {
            unsigned int codepoint = 0;
            const auto bytes = ImTextCharFromUtf8(&codepoint, it, end);
            if (codepoint == '\\')
            {
                display += "\\\\";
            }
            else if (codepoint >= 0x20 && !(codepoint >= 0x7f && codepoint <= 0x9f) &&
                     font->IsGlyphInFont(static_cast<ImWchar>(codepoint)))
            {
                display.append(it, bytes);
            }
            else
            {
                char digits[6]; // Unicode 标量至多需要六位十六进制数字.
                const auto converted = std::to_chars(digits, digits + sizeof(digits), codepoint, 16);
                display += "\\u{";
                display.append(digits, converted.ptr);
                display += '}';
            }
            it += bytes;
        }
        return display;
    }

    // 菜单读已发布的宿主设置. 同一轮可产生两个各自携带窗口身份的值请求.
    void Workspace::DrawWindowMenu(const Platform::WindowSettings *window, WorkspaceFrame &frame, bool allowActions)
    {
        if (!window || !ImGui::BeginMenu("Window"))
        {
            m_TitleEditing = false;
            return;
        }
        const bool requested = window->requestedSwapInterval != 0;
        if (ImGui::MenuItem("VSync", nullptr, requested) && allowActions)
        {
            frame.vsync = Platform::SetWindowVSync{window->window, !requested};
        }
        if (window->effectiveSwapInterval)
        {
            ImGui::Text("Effective interval %d", *window->effectiveSwapInterval);
        }
        else
        {
            ImGui::TextUnformatted("Effective interval unavailable");
        }
        if (window->swapIntervalFailure)
        {
            ImGui::TextUnformatted(window->swapIntervalFailure->View().data());
        }

        // 草稿只在输入项持有键盘期间保留. Enter 提交, 失焦或关闭菜单丢弃草稿.
        if (!m_TitleEditing || !allowActions)
        {
            m_TitleDraft.fill('\0');
            if (window->requestedTitle.size() < m_TitleDraft.size())
            {
                std::ranges::copy(window->requestedTitle, m_TitleDraft.begin());
            }
        }
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::InputText("Title", m_TitleDraft.data(), m_TitleDraft.size(), ImGuiInputTextFlags_EnterReturnsTrue) &&
            allowActions)
        {
            frame.title = Platform::SetWindowTitle{window->window, m_TitleDraft.data()};
        }
        m_TitleEditing = allowActions && ImGui::IsItemActive();
        // 草稿需要转义时补一行可核对的预览. 草稿预览和宿主回执采用同一显示规则.
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 280.0f);
        if (m_TitleEditing)
        {
            const std::string_view draft(m_TitleDraft.data());
            const auto display = DisplayWindowTitle(draft);
            if (display != draft)
            {
                ImGui::TextUnformatted("Draft title");
                ImGui::TextUnformatted(display.c_str());
            }
        }
        ImGui::Text("Requested title %s", DisplayWindowTitle(window->requestedTitle).c_str());
        if (window->effectiveTitle)
        {
            ImGui::Text("Effective title %s", DisplayWindowTitle(*window->effectiveTitle).c_str());
        }
        else
        {
            ImGui::TextUnformatted("Effective title unavailable");
        }
        ImGui::PopTextWrapPos();
        if (window->titleFailure)
        {
            ImGui::TextUnformatted(window->titleFailure->View().data());
        }
        ImGui::EndMenu();
    }

    // 主视口使用会话快照. 其余面板的展示开关由 Workspace 持有.
    bool Workspace::Visible(ActionId action, EditState edits) const noexcept
    {
        return action == ActionId::ToggleViewport ? edits.viewportVisible : m_Visible[static_cast<std::size_t>(action)];
    }

    // 菜单与面板关闭共享此入口. 主视口操作交由帧边界的历史应用处理.
    void Workspace::SetVisible(ActionId action, bool visible, WorkspaceFrame &frame)
    {
        if (action == ActionId::ToggleViewport)
        {
            frame.edits.emplace_back(SetViewportVisibility{visible});
        }
        else
        {
            m_Visible[static_cast<std::size_t>(action)] = visible;
        }
    }

    // 文件操作、历史和删除统一使用当前快照的启用条件. 实际应用仍由 owner 核对身份.
    bool Workspace::CanActivate(ActionId action, const WorkspaceState &state) noexcept
    {
        if (!state.allowActions)
        {
            return false;
        }
        switch (action)
        {
        case ActionId::Undo: return state.edits.canUndo && !state.navigating;
        case ActionId::Redo: return state.edits.canRedo && !state.navigating;
        case ActionId::DeleteSelection:
            return state.scene && state.view && state.selection && !state.frozen && !state.navigating;
        case ActionId::ImportModel:
            return state.document && !state.frozen && !state.navigating && !state.model.Pending();
        case ActionId::NewScene:
        case ActionId::OpenScene:
        case ActionId::SaveScene:
        case ActionId::SaveSceneAs:
            return state.document && state.document->operation == 0 && !state.navigating;
        default:
            return true;
        }
    }

    // 从统一身份生成布局、历史或文件意图. 菜单和快捷键对相同快照遵循同一入口.
    void Workspace::Activate(ActionId action, const WorkspaceState &state, WorkspaceFrame &frame)
    {
        if (!CanActivate(action, state))
        {
            return;
        }
        using enum ActionId;
        switch (action)
        {
        case ToggleHierarchy:
        case ToggleInspector:
        case ToggleViewSettings:
        case ToggleViewport:
            SetVisible(action, !Visible(action, state.edits), frame);
            break;
        case NewScene: m_Documents.Request(SceneFileAction::New, *state.document, frame); break;
        case OpenScene: m_Documents.Request(SceneFileAction::Open, *state.document, frame); break;
        case SaveScene: m_Documents.Request(SceneFileAction::Save, *state.document, frame); break;
        case SaveSceneAs: m_Documents.Request(SceneFileAction::SaveAs, *state.document, frame); break;
        case ImportModel: m_Documents.RequestImport(state.document->version.scene); break;
        case DeleteSelection: frame.edits.emplace_back(Scene::EraseSubtree{*state.selection}); break;
        case Undo: frame.edits.emplace_back(HistoryAction::Undo); break;
        case Redo: frame.edits.emplace_back(HistoryAction::Redo); break;
        }
    }

    // 使用动作表生成菜单, 由状态快照统一控制可用性.
    void Workspace::DrawActions(const WorkspaceState &state, WorkspaceFrame &frame)
    {
        const auto menu = [&](const char *name, std::span<const CommandItem> items)
        {
            if (ImGui::BeginMenu(name))
            {
                for (const auto &item : items)
                {
                    if (ImGui::MenuItem(item.label, item.shortcut, false, CanActivate(item.action, state)))
                    {
                        Activate(item.action, state, frame);
                    }
                }
                ImGui::EndMenu();
            }
        };
        menu("File", FileItems);
        menu("Edit", EditItems);
        if (ImGui::BeginMenu("View"))
        {
            for (const auto &panel : Panels)
            {
                const bool visible = Visible(panel.action, state.edits);
                if (ImGui::MenuItem(panel.menu, panel.shortcut, visible, CanActivate(panel.action, state)))
                {
                    Activate(panel.action, state, frame);
                }
            }
            ImGui::EndMenu();
        }
    }

    // 布局和诊断只读. 封存后由会话应用字段、文件和视口意图.
    WorkspaceFrame Workspace::Draw(const WorkspaceState &state, const Viewport &viewport)
    {
        WorkspaceFrame frame;
        const auto *acceptedPreview = state.view ? state.view->AcceptedScenePreview() : nullptr;
        const auto &frameDiagnostic = state.frameDiagnostic;
        if (ImGui::BeginMainMenuBar())
        {
            DrawActions(state, frame);
            DrawWindowMenu(state.window, frame, state.allowActions);
            if (state.document)
            {
                m_Documents.DrawStatus(*state.document, state.model, state.fileError, state.allowActions, frame);
            }
            const bool degraded = acceptedPreview && acceptedPreview->degradation.has_value();
            if ((frameDiagnostic || degraded) && ImGui::BeginMenu("Frame status"))
            {
                // 使用窗口内容范围为正文换行, 不受 Viewport 面板宽度约束. 高度超出时沿菜单滚动.
                const auto &style = ImGui::GetStyle();
                const float available = ImGui::GetWindowViewport()->WorkSize.x -
                    2.0f * (style.DisplaySafeAreaPadding.x + style.WindowPadding.x) - style.ScrollbarSize;
                const float width = std::max(1.0f, std::min(40.0f * ImGui::GetFontSize(), available));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
                if (frameDiagnostic)
                {
                    const auto recovery = frameDiagnostic->recovery;
                    ImGui::TextUnformatted(recovery == FrameRecovery::Normal ? "Normal rendering recovered" :
                        recovery == FrameRecovery::SafeUi ? "Safe UI - retaining the last accepted output" :
                        recovery == FrameRecovery::Cancelled ? "Frame cancelled - retrying on the next iteration" :
                                                              "Rendering stopped");
                    ImGui::Separator();
                    ImGui::TextUnformatted(frameDiagnostic->primary.View().data());
                    if (frameDiagnostic->additional)
                    {
                        ImGui::TextUnformatted(frameDiagnostic->additional->View().data());
                    }
                }
                if (degraded)
                {
                    if (frameDiagnostic)
                    {
                        ImGui::Separator();
                    }
                    // 通过接受的像素代读取摘要和实际模式. 帧失败诊断仍沿原入口保留.
                    ImGui::Text("Last accepted Forward preview - %s",
                                PreviewNames[static_cast<std::size_t>(acceptedPreview->mode)]);
                    const auto summary = acceptedPreview->degradation->View();
                    ImGui::TextUnformatted(summary.data(), summary.data() + summary.size());
                }
                ImGui::PopTextWrapPos();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // 路径与未保存确认也在当前 UI 帧参与归属判定, 不从快捷键回调直接启动后台任务.
        m_Documents.DrawDialog(state.document, state.allowActions, frame);
        const bool allowEdits = state.allowActions && !state.frozen && !state.navigating;
        const auto drawEditError = [&]()
        {
            if (state.editError)
            {
                ImGui::TextWrapped("%s", state.editError->View().data());
            }
        };
        // 本轮主视口布局沿用传入快照, 显隐编辑在封存后生效.
        const auto dock = ImGui::DockSpaceOverViewport();
        bool inspectorDrawn = false;
        for (const auto &panel : Panels)
        {
            bool visible = Visible(panel.action, state.edits);
            if (!visible)
            {
                continue;
            }
            ImGui::SetNextWindowDockID(dock, ImGuiCond_FirstUseEver);
            if (ImGui::Begin(panel.window, &visible) && visible)
            {
                switch (panel.action)
                {
                case ActionId::ToggleViewport:
                    frame.viewport = viewport.Draw();
                    break;
                case ActionId::ToggleHierarchy:
                    m_ScenePanels.DrawHierarchy(state.scene, state.selection, state.allowActions, frame);
                    break;
                case ActionId::ToggleInspector:
                    inspectorDrawn = true;
                    m_ScenePanels.DrawInspector(state.scene, state.selection, allowEdits, frame);
                    drawEditError();
                    break;
                case ActionId::ToggleViewSettings:
                    if (state.view)
                    {
                        m_ScenePanels.DrawCamera(state.scene, *state.view, allowEdits, frame);
                        if (!state.view->DocumentRequests().cameraEntityId)
                        {
                            ImGui::TextWrapped("Right mouse + WASD/QE: Fly. Alt + left mouse: Orbit. Middle mouse: Pan.");
                        }
                        const auto settings = state.view->Settings();
                        int preview = static_cast<int>(settings.preview);
                        ImGui::BeginDisabled(!state.allowActions);
                        const bool changed = ImGui::Combo("Preview", &preview, PreviewNames, IM_ARRAYSIZE(PreviewNames));
                        ImGui::EndDisabled();
                        if (changed)
                        {
                            frame.preview = static_cast<Renderer::PreviewMode>(preview);
                        }
                        DrawViewSettings(settings, allowEdits, frame);
                        drawEditError();
                    }
                    break;
                default:
                    break;
                }
            }
            ImGui::End();
            if (!visible && state.allowActions)
            {
                SetVisible(panel.action, false, frame);
            }
        }
        if (!inspectorDrawn)
        {
            EndRotationInput();
        }
        return frame;
    }

    // 只派发有资格的 Press. 事件次序来自会话, 文件输入和控件不在这里重新消费.
    void Workspace::ApplyShortcut(const Platform::InputEvent &event, const InputClaims &claims,
                                  const WorkspaceState &state, WorkspaceFrame &frame)
    {
        if (ResolveInputOwner(claims, event) != InputOwner::Workspace)
        {
            return;
        }
        const auto &key = std::get<Platform::KeyEvent>(event);
        if (key.action != Platform::KeyAction::Press || key.key == Platform::Key::Unknown)
        {
            return;
        }
        const auto modifiers = static_cast<unsigned>(key.modifiers) & ShortcutModifiers;
        const auto dispatch = [&](std::span<const CommandItem> items)
        {
            const auto item = std::ranges::find_if(items, [&](const CommandItem &command)
            {
                return command.key == key.key && command.modifiers == modifiers;
            });
            if (item != items.end())
            {
                Activate(item->action, state, frame);
                return true;
            }
            return false;
        };
        if (modifiers == (Control | Shift) && key.key == Platform::Key::I)
        {
            Activate(ActionId::ToggleInspector, state, frame);
        }
        else if (!dispatch(FileItems))
        {
            static_cast<void>(dispatch(EditItems));
        }
    }
}
