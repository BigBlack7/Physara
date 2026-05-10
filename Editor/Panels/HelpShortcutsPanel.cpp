#include "HelpShortcutsPanel.hpp"

#include <string>
#include <vector>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Help / Shortcuts";

        std::vector<std::string> CollectGroups(const ShortcutRegistry &shortcutRegistry)
        {
            std::vector<std::string> groups;
            for (const ShortcutAction &action : shortcutRegistry.GetActions())
            {
                bool exists = false;
                for (const std::string &group : groups)
                {
                    if (group == action.group)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    groups.push_back(action.group);
                }
            }

            return groups;
        }
    }

    HelpShortcutsPanel::HelpShortcutsPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry)
        : m_Context(context), m_ShortcutRegistry(shortcutRegistry)
    {
    }

    void HelpShortcutsPanel::Draw()
    {
        if (!m_Context.ui.showHelpShortcuts)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(760.f, 520.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(Internal::PanelName, &m_Context.ui.showHelpShortcuts, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Available editor shortcuts");
        ImGui::Separator();

        if (ImGui::BeginTable("ShortcutTable", 4,
                              ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 130.f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 96.f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableHeadersRow();

            const std::vector<std::string> groups = Internal::CollectGroups(m_ShortcutRegistry);
            for (const std::string &group : groups)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SeparatorText(group.c_str());

                for (const ShortcutAction *action : m_ShortcutRegistry.GetActionsByGroup(group))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(action->name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(action->keyChord.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(action->implemented ? "Active" : "Planned");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextWrapped("%s", action->description.c_str());
                }
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }
}