#include "InspectorPanel.hpp"

#include <cstdint>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Inspector";
    }

    InspectorPanel::InspectorPanel(EditorContext &context)
        : m_Context(context)
    {
    }

    void InspectorPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        if (m_Context.activeScene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        if (m_Context.selectedEntity == Engine::NullEntity)
        {
            ImGui::TextUnformatted("No entity selected.");
            ImGui::End();
            return;
        }

        ImGui::Text("Selected Entity: %u",
                    static_cast<std::uint32_t>(m_Context.selectedEntity));

        ImGui::Separator();
        ImGui::TextUnformatted("Components will appear here.");

        ImGui::End();
    }
}