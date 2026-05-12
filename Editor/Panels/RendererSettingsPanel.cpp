#include "RendererSettingsPanel.hpp"

#include <algorithm>

#include <Engine/Core/Log.hpp>
#include <glm/vec3.hpp>
#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Renderer Settings";

        static constexpr const char *PipelineLabels[] = {
            "Forward",
            "ForwardPlus",
            "Deferred"};

        static constexpr const char *DebugViewLabels[] = {
            "None",
            "Normals",
            "Depth",
            "Wireframe",
            "LightComplexity",
            "GBufferAlbedo",
            "GBufferNormal",
            "GBufferRoughness",
            "GBufferMetallic",
            "GBufferAO",
            "GBufferEmissive",
            "ShadowMap"};

        static constexpr const char *CaptureFormatLabels[] = {
            "PNG",
            "JPG",
            "EXR (planned)"};
    }

    RendererSettingsPanel::RendererSettingsPanel(EditorContext &context, EditorCamera &camera)
        : m_Context(context), m_Camera(camera) {}

    void RendererSettingsPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        DrawCameraSection();
        DrawShadowSection();
        DrawPostProcessSection();
        DrawPipelineSection();
        DrawDebugVisualizationSection();
        DrawEnvironmentSection();

        ImGui::End();
    }

    void RendererSettingsPanel::DrawCameraSection()
    {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            EditorCameraSettings &camera = m_Camera.GetSettings();
            glm::vec3 position = m_Camera.GetPosition();
            if (ImGui::DragFloat3("Position", &position.x, 0.05f))
            {
                m_Camera.SetPosition(position);
            }

            float yawPitch[2] = {m_Camera.GetYawDegrees(), m_Camera.GetPitchDegrees()};
            if (ImGui::DragFloat2("Yaw / Pitch", yawPitch, 0.25f))
            {
                m_Camera.SetYawPitchDegrees(yawPitch[0], yawPitch[1]);
            }

            ImGui::DragFloat("Focal Length", &camera.focalLengthMillimeters, 0.25f, 1.f, 300.f, "%.1f mm");
            ImGui::DragFloat("Aperture", &camera.apertureFStop, 0.05f, 0.1f, 64.f, "f/%.1f");
            ImGui::DragFloat("Shutter", &camera.shutterTimeSeconds, 0.0005f, 1.f / 32000.f, 30.f, "%.4f s");
            ImGui::DragFloat("ISO", &camera.iso, 1.f, 1.f, 25600.f, "%.0f");
            ImGui::DragFloat("Near Clip", &camera.nearClipMeters, 0.01f, 0.001f, 100.f, "%.3f m");
            ImGui::DragFloat("Far Clip", &camera.farClipMeters, 1.f, 0.01f, 100000.f, "%.1f m");
            ImGui::DragFloat("Fly Speed", &camera.flySpeedMetersPerSecond, 0.1f, 0.1f, 200.f, "%.1f m/s");

            const char *sourceLabels[] = {"Editor Camera", "Scene Camera"};
            int sourceIndex = static_cast<int>(m_Camera.GetCaptureViewSource());
            if (ImGui::Combo("Capture View", &sourceIndex, sourceLabels, IM_ARRAYSIZE(sourceLabels)))
            {
                m_Camera.SetCaptureViewSource(static_cast<CaptureViewSource>(sourceIndex));
            }

            ImGui::Text("EV100: %.2f", m_Camera.GetEV100());
            ImGui::Text("Mode: %s", m_Camera.GetMode() == EditorCameraMode::Fly ? "Fly" : "Orbit");
            ImGui::SeparatorText("Capture");

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
                const Engine::RenderView captureView =
                    m_Camera.BuildCaptureView(m_Context.activeScene, m_Context.selectedEntity,
                                              m_Camera.GetCaptureViewSource());
                PHYSARA_INFO("Capture requested from Renderer Settings. Viewport={}x{}, EV100={:.2f}. Renderer capture output will be connected in Phase 4.",
                             captureView.viewport.width, captureView.viewport.height, captureView.ev100);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Shortcut: F12");
        }
    }

    void RendererSettingsPanel::DrawShadowSection()
    {
        if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("Shadow controls are reserved for the renderer path.");
            ImGui::TextUnformatted("Reserved: map size, cascade count, bias and PCF kernel.");
        }
    }

    void RendererSettingsPanel::DrawPostProcessSection()
    {
        if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("Post process controls are reserved for the final composite pass.");
            ImGui::TextUnformatted("Reserved: tone mapping, exposure, bloom and FXAA.");
        }
    }

    void RendererSettingsPanel::DrawPipelineSection()
    {
        if (!ImGui::CollapsingHeader("Pipeline", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        int pipelineIndex = static_cast<int>(m_Context.settings.pipelineMode);
        if (ImGui::Combo("Mode", &pipelineIndex, Internal::PipelineLabels, IM_ARRAYSIZE(Internal::PipelineLabels)))
        {
            m_Context.settings.pipelineMode = static_cast<RenderPipelineMode>(pipelineIndex);
        }

        ImGui::TextUnformatted("Current selection is an editor setting placeholder until Renderer is connected.");
    }

    void RendererSettingsPanel::DrawDebugVisualizationSection()
    {
        if (!ImGui::CollapsingHeader("Debug Visualization", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        int debugIndex = static_cast<int>(m_Context.settings.debugViewMode);
        if (ImGui::Combo("View", &debugIndex, Internal::DebugViewLabels, IM_ARRAYSIZE(Internal::DebugViewLabels)))
        {
            m_Context.settings.debugViewMode = static_cast<DebugViewMode>(debugIndex);
        }

        ImGui::TextUnformatted("Debug output will replace Scene View final color when Renderer supports it.");
    }

    void RendererSettingsPanel::DrawEnvironmentSection()
    {
        if (ImGui::CollapsingHeader("IBL / Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("Environment lighting and atmosphere controls are reserved.");
            ImGui::TextUnformatted("Reserved: HDR environment, IBL precompute and atmosphere preset.");
        }
    }
}