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
        Panel{ActionId::ToggleViewport, "Viewport", "Viewport###Physara.Viewport", nullptr}};

    // 空场景提供层级入口, 实体创建等到有对应领域数据时才启用.
    static void DrawHierarchy()
    {
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::BeginDisabled();
        ImGui::Button("Create Entity");
        ImGui::EndDisabled();
    }

    // 将 View 提供的等待和拒绝原因映射为 Inspector 标签.
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
        }
        return "";
    }

    // 用权威比例快照初始化本轮控件, 从 ImGui 项状态提取比例意图.
    static void DrawInspector(const std::optional<SelectionId> &selection, const Renderer::ViewSettings &view,
                              const std::optional<Core::BoundedText> &editError, bool allowActions,
                              WorkspaceFrame &frame)
    {
        ImGui::TextUnformatted(selection ? "Selection data pending." : "No selection.");
        ImGui::BeginDisabled();
        ImGui::Button("Edit Transform");
        ImGui::EndDisabled();
        if (!view.view.IsValid())
        {
            return;
        }
        ImGui::SeparatorText("Main View");
        constexpr char scaleLabel[] = "Render scale";
        auto scale = view.requestedScale;
        const auto &context = *ImGui::GetCurrentContext();
        const auto slider = ImGui::GetID(scaleLabel);
        const bool sampleRelease = allowActions && context.ActiveId == slider &&
                                   context.ActiveIdSource == ImGuiInputSource_Mouse &&
                                   ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsMousePosValid();
        auto &mouseDown = ImGui::GetIO().MouseDown[ImGuiMouseButton_Left];
        if (sampleRelease)
        {
            // 固定版 SliderBehavior 在释放时先清除活动项. 暂时保持本滑块的按下状态,
            // 让它用原有抓取偏移和舍入计算末段位置. 真实的释放边沿仍供手势判断.
            mouseDown = true;
        }
        const bool changed = ImGui::SliderFloat(scaleLabel, &scale, Renderer::MinRenderScale,
                                                Renderer::MaxRenderScale, "%.2fx", ImGuiSliderFlags_NoInput);
        if (sampleRelease)
        {
            // 先恢复真实按钮状态并结束活动项, 再读取项状态以提交本次末段采样.
            mouseDown = false;
            if (context.ActiveId == slider)
            {
                ImGui::ClearActiveID();
            }
        }
        if (allowActions)
        {
            // 释放和导航确认作为正常结束信号, Escape 通过手势的取消标志处理.
            const bool began = ImGui::IsItemActivated();
            const bool active = ImGui::IsItemActive();
            const bool deactivated = ImGui::IsItemDeactivated();
            const bool finished = deactivated && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
            frame.scale = {view.view, changed ? std::optional{scale} : std::nullopt, began, active, finished,
                           ImGui::IsKeyPressed(ImGuiKey_Escape, false)};
            if (changed && !began && !active && !deactivated)
            {
                frame.edits.emplace_back(SetViewScale{view.view, scale});
            }
        }
        ImGui::Text("Requested %.2fx", view.requestedScale);
        if (view.effective)
        {
            ImGui::Text("Effective %.2fx (%u x %u)", view.effective->scale,
                        view.effective->extent.width, view.effective->extent.height);
        }
        else
        {
            ImGui::TextUnformatted("Effective - awaiting confirmation");
        }
        ImGui::TextUnformatted(ViewStatus(view.reason));
        if (editError)
        {
            ImGui::TextUnformatted(editError->View().data());
        }
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

    // 主视口使用会话快照. 另外两个面板的展示开关由 Workspace 持有.
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

    WorkspaceFrame Workspace::Draw(const std::optional<SelectionId> &selection, const Viewport &viewport,
                                    EditState edits, const Renderer::ViewSettings &view,
                                    const Platform::WindowSettings *window,
                                    const std::optional<Core::BoundedText> &editError, bool allowActions)
    {
        WorkspaceFrame frame;
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, edits.canUndo && allowActions))
                {
                    frame.edits.emplace_back(HistoryAction::Undo);
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, edits.canRedo && allowActions))
                {
                    frame.edits.emplace_back(HistoryAction::Redo);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                for (const auto &panel : Panels)
                {
                    const bool visible = Visible(panel.action, edits);
                    if (ImGui::MenuItem(panel.menu, panel.shortcut, visible) && allowActions)
                    {
                        SetVisible(panel.action, !visible, frame);
                    }
                }
                ImGui::EndMenu();
            }
            DrawWindowMenu(window, frame, allowActions);
            ImGui::EndMainMenuBar();
        }

        // 本轮主视口布局沿用传入快照. 显隐意图在本轮 UI 封存后更新会话.
        const auto dock = ImGui::DockSpaceOverViewport();
        for (const auto &panel : Panels)
        {
            bool visible = Visible(panel.action, edits);
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
                    DrawHierarchy();
                    break;
                case ActionId::ToggleInspector:
                    DrawInspector(selection, view, editError, allowActions, frame);
                    break;
                }
            }
            ImGui::End();
            if (!visible && allowActions)
            {
                SetVisible(panel.action, false, frame);
            }
        }
        return frame;
    }

    void Workspace::ApplyShortcuts(std::span<const Platform::InputEvent> events, const InputClaims &claims,
                                   WorkspaceFrame &frame)
    {
        // 锁定键不参与组合. 先沿当前输入归属筛掉文本, 弹层和捕获, 再匹配首次按下.
        using Platform::Modifier;
        constexpr auto control = static_cast<unsigned>(Modifier::Control);
        constexpr auto shift = static_cast<unsigned>(Modifier::Shift);
        constexpr auto chord = control | shift | static_cast<unsigned>(Modifier::Alt) |
                               static_cast<unsigned>(Modifier::Super);
        for (const auto &event : events)
        {
            if (ResolveInputOwner(claims, event) != InputOwner::Workspace)
            {
                continue;
            }
            const auto &key = std::get<Platform::KeyEvent>(event);
            if (key.action != Platform::KeyAction::Press)
            {
                continue;
            }
            const auto modifiers = static_cast<unsigned>(key.modifiers) & chord;
            if (modifiers == (control | shift) && key.key == Platform::Key::I)
            {
                auto &visible = m_Visible[static_cast<std::size_t>(ActionId::ToggleInspector)];
                visible = !visible;
            }
            else if (modifiers == control)
            {
                // 依事件顺序交给历史 owner. 同批较早的动作可能改变下一次重放的可用性.
                if (key.key == Platform::Key::Z)
                {
                    frame.edits.emplace_back(HistoryAction::Undo);
                }
                else if (key.key == Platform::Key::Y)
                {
                    frame.edits.emplace_back(HistoryAction::Redo);
                }
            }
        }
    }
}
