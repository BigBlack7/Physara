#include "InspectorPanel.hpp"
#include "ComponentDrawer.hpp"

#include <algorithm>
#include <cstdint>

#include <imgui/imgui.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Inspector";

        static constexpr const char *CaptureFormatLabels[] = {
            "PNG",
            "JPG",
            "EXR (planned)"};
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
        DrawCameraCaptureSection(entity);
        TryDrawComponent<Engine::MeshComponent>(entity, "Mesh");
        TryDrawComponent<Engine::MaterialComponent>(entity, "Material");
    }

    void InspectorPanel::DrawCameraCaptureSection(Engine::Entity entity)
    {
        if (!entity.HasComponent<Engine::CameraComponent>())
        {
            return;
        }

        if (!ImGui::CollapsingHeader("Capture", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::PushID("CameraCapture");

        ImGui::InputText("File Prefix",
                         m_Context.settings.capture.fileNamePrefix.data(),
                         m_Context.settings.capture.fileNamePrefix.size());
        ImGui::Combo("Format", &m_Context.settings.capture.fileFormatIndex,
                     Internal::CaptureFormatLabels, IM_ARRAYSIZE(Internal::CaptureFormatLabels));

        m_Context.settings.capture.resolutionScale =
            std::clamp(m_Context.settings.capture.resolutionScale, 0.25f, 4.f);
        ImGui::SliderFloat("Resolution Scale", &m_Context.settings.capture.resolutionScale, 0.25f, 4.f, "%.2fx");
        ImGui::Checkbox("Include Debug View", &m_Context.settings.capture.includeDebugView);

        if (ImGui::Button("Capture Current View"))
        {
            m_Context.settings.capture.captureRequested = true;
            const auto &camera = entity.GetComponent<Engine::CameraComponent>();
            PHYSARA_INFO("Capture requested from Scene Camera Inspector. EV100={:.2f}.", camera.GetEV100());
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Shortcut: F12");

        ImGui::PopID();
    }
}