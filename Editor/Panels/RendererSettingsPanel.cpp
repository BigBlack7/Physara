#include "RendererSettingsPanel.hpp"

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        static constexpr const char *kPipelineLabels[] = {
            "Forward",
            "ForwardPlus",
            "Deferred"};

        static constexpr const char *kDebugViewLabels[] = {
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
        ImGui::Begin("Renderer Settings");

        ImGui::SeparatorText("Pipeline");
        {
            int pipelineIndex = static_cast<int>(m_Context.settings.pipelineMode);
            if (ImGui::Combo("Mode", &pipelineIndex, Internal::kPipelineLabels, IM_ARRAYSIZE(Internal::kPipelineLabels)))
            {
                m_Context.settings.pipelineMode = static_cast<RenderPipelineMode>(pipelineIndex);
            }
        }

        ImGui::SeparatorText("Debug Visualization");
        {
            int debugIndex = static_cast<int>(m_Context.settings.debugViewMode);
            if (ImGui::Combo("View", &debugIndex, Internal::kDebugViewLabels, IM_ARRAYSIZE(Internal::kDebugViewLabels)))
            {
                m_Context.settings.debugViewMode = static_cast<DebugViewMode>(debugIndex);
            }
        }

        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::TextUnformatted("Coming soon.");
        }
        if (ImGui::CollapsingHeader("Shadow"))
        {
            ImGui::TextUnformatted("Coming soon.");
        }
        if (ImGui::CollapsingHeader("Post Process"))
        {
            ImGui::TextUnformatted("Coming soon.");
        }
        if (ImGui::CollapsingHeader("IBL / Atmosphere"))
        {
            ImGui::TextUnformatted("Coming soon.");
        }

        ImGui::End();
    }
}