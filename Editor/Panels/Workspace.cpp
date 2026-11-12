#include "Workspace.hpp"

#include <variant>

#include <imgui.h>

#include "Session/ActionRouter.hpp"

/// @file
/// @brief 在主窗口组装停靠区, 固定面板和显隐动作.
/// 面板从可用的只读输入构建展示, 领域数据缺失时显示状态并禁用相应控件.

namespace Physara::Editor::Detail
{
    /// @brief 面板注册项. action 同时标识布局动作和显隐槽位, shortcut 提供菜单中的提示文字.
    struct Panel
    {
        ActionId action;
        const char *menu;
        const char *window;
        const char *shortcut;
    };

    // ### 后的稳定 ID 供 ImGui 关联停靠设置. 首次停靠组选择最后加入的 Viewport 页.
    static constexpr std::array Panels{
        Panel{ActionId::ToggleHierarchy, "Hierarchy", "Hierarchy###Physara.Hierarchy", nullptr},
        Panel{ActionId::ToggleInspector, "Inspector", "Inspector###Physara.Inspector", "Ctrl+Shift+I"},
        Panel{ActionId::ToggleViewport, "Viewport", "Viewport###Physara.Viewport", nullptr}};

    // 空场景下展示面板入口, 创建行为由后续领域消费者提供.
    static void DrawHierarchy()
    {
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::BeginDisabled();
        ImGui::Button("Create Entity");
        ImGui::EndDisabled();
    }

    // 选择仅用于只读显示. 本阶段还没有可提供变换字段的领域对象.
    static void DrawInspector(const std::optional<SelectionId> &selection)
    {
        ImGui::TextUnformatted(selection ? "Selection data pending." : "No selection.");
        ImGui::BeginDisabled();
        ImGui::Button("Edit Transform");
        ImGui::EndDisabled();
    }

    ViewportLayout Workspace::Draw(const std::optional<SelectionId> &selection, const Viewport &viewport,
                                    const Platform::WindowState &window)
    {
        // 先提交主窗口菜单. 菜单动作当场更新显隐, 随后的面板构建使用更新后的状态.
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("View"))
            {
                for (const auto &panel : Panels)
                {
                    if (ImGui::MenuItem(panel.menu, panel.shortcut, m_Visible[static_cast<std::size_t>(panel.action)]))
                    {
                        Toggle(panel.action);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        const auto dock = ImGui::DockSpaceOverViewport();
        // 本轮布局从空值开始. 隐藏或被停靠页遮住的视口不会沿用前一轮请求.
        ViewportLayout layout;
        for (const auto &panel : Panels)
        {
            auto &visible = m_Visible[static_cast<std::size_t>(panel.action)];
            if (!visible)
            {
                continue;
            }
            // 首次出现时进入默认停靠区, 后续沿用 context 记录的布局.
            ImGui::SetNextWindowDockID(dock, ImGuiCond_FirstUseEver);
            if (ImGui::Begin(panel.window, &visible))
            {
                switch (panel.action)
                {
                case ActionId::ToggleViewport:
                    layout = viewport.Draw(window);
                    break;
                case ActionId::ToggleHierarchy:
                    DrawHierarchy();
                    break;
                case ActionId::ToggleInspector:
                    DrawInspector(selection);
                    break;
                default:
                    break;
                }
            }
            ImGui::End();
        }
        return layout;
    }

    void Workspace::ApplyShortcuts(std::span<const Platform::InputEvent> events, const InputClaims &claims) noexcept
    {
        // 用事件自身的修饰键匹配组合, 保留锁定键状态的独立含义. Repeat 继续留在原始事件流中.
        using Platform::Modifier;
        constexpr auto required = static_cast<unsigned>(Modifier::Control) | static_cast<unsigned>(Modifier::Shift);
        constexpr auto chord = required | static_cast<unsigned>(Modifier::Alt) | static_cast<unsigned>(Modifier::Super);
        for (const auto &event : events)
        {
            if (ResolveInputOwner(claims, event) != InputOwner::Workspace)
            {
                continue;
            }
            const auto &key = std::get<Platform::KeyEvent>(event);
            if (key.action == Platform::KeyAction::Press && key.key == Platform::Key::I &&
                (static_cast<unsigned>(key.modifiers) & chord) == required)
            {
                Toggle(ActionId::ToggleInspector);
            }
        }
    }

    void Workspace::Toggle(ActionId action) noexcept
    {
        auto &visible = m_Visible[static_cast<std::size_t>(action)];
        visible = !visible;
    }
}
