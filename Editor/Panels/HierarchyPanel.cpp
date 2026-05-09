#include "HierarchyPanel.hpp"

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Hierarchy";
    }

    HierarchyPanel::HierarchyPanel(EditorContext &context) : m_Context(context) {}

    void HierarchyPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        if (m_Context.activeScene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Scene hierarchy will appear here.");

        ImGui::End();
    }
}