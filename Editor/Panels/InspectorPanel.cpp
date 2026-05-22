#include "InspectorPanel.hpp"
#include "ComponentDrawer.hpp"

#include <algorithm>
#include <cstdint>

#include <imgui/imgui.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace InspectorPanelDetail
    {
        constexpr const char *PanelName = "Inspector";

        static constexpr const char *CaptureFormatLabels[] = {
            "PNG",
            "JPG"};
    }

    InspectorPanel::InspectorPanel(EditorContext &context, Engine::AssetManager &assetManager)
        : m_Context(context), m_AssetManager(assetManager)
    {
    }

    void InspectorPanel::Draw()
    {
        ImGui::Begin(InspectorPanelDetail::PanelName);

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
            m_Context.selectedEntities.clear();
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

        TryDrawComponent<Engine::TagComponent>(entity, "Tag", m_Context, &m_AssetManager);
        TryDrawComponent<Engine::TransformComponent>(entity, "Transform", m_Context, &m_AssetManager);
        TryDrawComponent<Engine::CameraComponent>(entity, "Camera", m_Context, &m_AssetManager);
        DrawCameraCaptureSection(entity);
        TryDrawComponent<Engine::LightComponent>(entity, "Light", m_Context, &m_AssetManager);
        TryDrawComponent<Engine::MeshComponent>(entity, "Mesh", m_Context, &m_AssetManager);
        TryDrawComponent<Engine::MaterialComponent>(entity, "Material", m_Context, &m_AssetManager);
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
        m_Context.settings.capture.fileFormatIndex = std::clamp(m_Context.settings.capture.fileFormatIndex, 0, 1);
        ImGui::Combo("Format", &m_Context.settings.capture.fileFormatIndex,
                     InspectorPanelDetail::CaptureFormatLabels, IM_ARRAYSIZE(InspectorPanelDetail::CaptureFormatLabels));

        m_Context.settings.capture.resolutionScale =
            std::clamp(m_Context.settings.capture.resolutionScale, 0.25f, 4.f);
        ImGui::SliderFloat("Resolution Scale", &m_Context.settings.capture.resolutionScale, 0.25f, 4.f, "%.2fx");

        if (ImGui::Button("Capture From This Camera"))
        {
            m_Context.settings.capture.captureRequested = true;
            const auto &camera = entity.GetComponent<Engine::CameraComponent>();
            PHYSARA_INFO("Camera capture requested. EV100={:.2f}.", camera.GetEV100());
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F12 captures this viewport camera.");

        ImGui::PopID();
    }
}