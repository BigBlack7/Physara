#include "RendererSettingsPanel.hpp"

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
    }

    RendererSettingsPanel::RendererSettingsPanel(EditorContext &context) : m_Context(context) {}

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
            ImGui::TextUnformatted("Editor camera exposure and physical camera parameters will appear here.");
            ImGui::TextUnformatted("Reserved: focal length, aperture, shutter, ISO, near/far planes.");
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