#include "InspectorPanel.hpp"
#include "ComponentDrawer.hpp"

#include <cstdint>

#include <imgui/imgui.h>

#include <Engine/Scene/Scene.hpp>

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

        if (m_Context.selectedEntity == Engine::NullEntity ||
            !m_Context.activeScene->IsValid(m_Context.selectedEntity))
        {
            m_Context.selectedEntity = Engine::NullEntity;
            ImGui::TextUnformatted("No entity selected.");
            ImGui::End();
            return;
        }

        DrawEntity(m_Context.activeScene->GetEntity(m_Context.selectedEntity));

        ImGui::End();
    }

    void InspectorPanel::DrawEntity(Engine::Entity entity)
    {
        if (!entity)
        {
            ImGui::TextUnformatted("No entity selected.");
            return;
        }

        ImGui::Text("Entity ID: %u", static_cast<std::uint32_t>(entity.GetHandle()));
        ImGui::Separator();

        TryDrawComponent<Engine::TagComponent>(entity, "Tag");
        TryDrawComponent<Engine::TransformComponent>(entity, "Transform");
        TryDrawComponent<Engine::CameraComponent>(entity, "Camera");
        TryDrawComponent<Engine::MeshComponent>(entity, "Mesh");
        TryDrawComponent<Engine::MaterialComponent>(entity, "Material");
    }
}