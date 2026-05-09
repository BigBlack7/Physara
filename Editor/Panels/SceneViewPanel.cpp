#include "SceneViewPanel.hpp"

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Scene View";
    }

    SceneViewPanel::SceneViewPanel(EditorContext &context) : m_Context(context) {}

    void SceneViewPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        ImGui::TextUnformatted("Gizmo:");
        ImGui::SameLine();
        int opIndex = static_cast<int>(m_Context.settings.gizmoOperation);
        ImGui::RadioButton("Translate", &opIndex, static_cast<int>(GizmoOperation::Translate));
        ImGui::SameLine();
        ImGui::RadioButton("Rotate", &opIndex, static_cast<int>(GizmoOperation::Rotate));
        ImGui::SameLine();
        ImGui::RadioButton("Scale", &opIndex, static_cast<int>(GizmoOperation::Scale));
        m_Context.settings.gizmoOperation = static_cast<GizmoOperation>(opIndex);

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextUnformatted("Space:");
        ImGui::SameLine();
        int spaceIndex = static_cast<int>(m_Context.settings.gizmoSpace);
        ImGui::RadioButton("Local", &spaceIndex, static_cast<int>(GizmoSpace::Local));
        ImGui::SameLine();
        ImGui::RadioButton("World", &spaceIndex, static_cast<int>(GizmoSpace::World));
        m_Context.settings.gizmoSpace = static_cast<GizmoSpace>(spaceIndex);

        ImGui::Separator();

        ImGui::BeginChild("SceneViewViewport", ImVec2(0.0f, 0.0f), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float width = (avail.x > 0.0f) ? avail.x : 0.0f;
        const float height = (avail.y > 0.0f) ? avail.y : 0.0f;

        m_Context.sceneView.sizeChanged =
            (width != m_Context.sceneView.width) ||
            (height != m_Context.sceneView.height);

        m_Context.sceneView.width = width;
        m_Context.sceneView.height = height;
        m_Context.sceneView.hovered = ImGui::IsWindowHovered();
        m_Context.sceneView.focused = ImGui::IsWindowFocused();

        ImGui::Text("Size: %.0f x %.0f", width, height);
        ImGui::Text("Hovered: %s", m_Context.sceneView.hovered ? "true" : "false");
        ImGui::Text("Focused: %s", m_Context.sceneView.focused ? "true" : "false");

        ImGui::Separator();
        ImGui::TextUnformatted("Render target preview will appear here.");

        ImGui::EndChild();

        ImGui::End();
    }
}