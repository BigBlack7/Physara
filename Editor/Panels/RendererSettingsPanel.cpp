#include "RendererSettingsPanel.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui/imgui.h>

#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Editor
{
    namespace RendererSettingsPanelDetail
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

        std::vector<std::filesystem::path> CollectEnvironmentMaps(const std::filesystem::path &assetsRoot)
        {
            std::vector<std::filesystem::path> maps;
            const std::filesystem::path envRoot = assetsRoot / "Textures" / "Env";
            std::error_code error{};
            if (!std::filesystem::exists(envRoot, error))
            {
                return maps;
            }

            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(envRoot, error))
            {
                if (error || !entry.is_regular_file(error))
                {
                    continue;
                }

                const std::string extension = Platform::FileSystem::GetExtensionLower(entry.path().string());
                if (extension == ".exr" || extension == ".hdr")
                {
                    maps.push_back(entry.path());
                }
            }

            std::sort(maps.begin(), maps.end());
            return maps;
        }

        std::filesystem::path ToAssetsRelative(const std::filesystem::path &assetsRoot, const std::filesystem::path &path)
        {
            std::error_code error{};
            std::filesystem::path relative = std::filesystem::relative(path, assetsRoot, error);
            return error ? path.lexically_normal() : relative.lexically_normal();
        }

    }

    RendererSettingsPanel::RendererSettingsPanel(EditorContext &context)
        : m_Context(context) {}

    void RendererSettingsPanel::Draw()
    {
        ImGui::Begin(RendererSettingsPanelDetail::PanelName);

        DrawShadowSection();
        DrawPostProcessSection();
        DrawPipelineSection();
        DrawDebugVisualizationSection();
        DrawEnvironmentSection();

        ImGui::End();
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
            ImGui::Checkbox("ACES Tone Mapping", &m_Context.settings.postProcess.toneMappingEnabled);
            ImGui::Checkbox("FXAA", &m_Context.settings.postProcess.fxaaEnabled);
            ImGui::Checkbox("Bloom", &m_Context.settings.postProcess.bloomEnabled);
            ImGui::BeginDisabled(!m_Context.settings.postProcess.bloomEnabled);
            ImGui::SliderFloat("Threshold", &m_Context.settings.postProcess.bloomThreshold, 0.f, 10.f, "%.2f");
            ImGui::SliderFloat("Knee", &m_Context.settings.postProcess.bloomKnee, 0.f, 2.f, "%.2f");
            ImGui::SliderFloat("Intensity", &m_Context.settings.postProcess.bloomIntensity, 0.f, 1.f, "%.3f");
            ImGui::SliderFloat("Radius", &m_Context.settings.postProcess.bloomRadius, 1.f, 8.f, "%.1f");
            ImGui::EndDisabled();
        }
    }

    void RendererSettingsPanel::DrawPipelineSection()
    {
        if (!ImGui::CollapsingHeader("Pipeline", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        int pipelineIndex = static_cast<int>(m_Context.settings.pipelineMode);
        if (ImGui::Combo("Mode", &pipelineIndex, RendererSettingsPanelDetail::PipelineLabels, IM_ARRAYSIZE(RendererSettingsPanelDetail::PipelineLabels)))
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
        if (ImGui::Combo("View", &debugIndex, RendererSettingsPanelDetail::DebugViewLabels, IM_ARRAYSIZE(RendererSettingsPanelDetail::DebugViewLabels)))
        {
            m_Context.settings.debugViewMode = static_cast<DebugViewMode>(debugIndex);
        }

        ImGui::TextUnformatted("Debug output will replace Scene View final color when Renderer supports it.");
    }

    void RendererSettingsPanel::DrawEnvironmentSection()
    {
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Skybox", &m_Context.settings.environment.skyboxEnabled);
            ImGui::SliderFloat("Skybox EV", &m_Context.settings.environment.skyboxExposureCompensation, -8.f, 8.f, "%.2f");

            const std::vector<std::filesystem::path> maps =
                RendererSettingsPanelDetail::CollectEnvironmentMaps(m_Context.assetsRootPath);
            std::string currentLabel = "None";
            if (!m_Context.settings.environment.skyboxPath.empty())
            {
                currentLabel = m_Context.settings.environment.skyboxPath.generic_string();
            }

            if (ImGui::BeginCombo("Panorama", currentLabel.c_str()))
            {
                const bool noneSelected = m_Context.settings.environment.skyboxPath.empty();
                if (ImGui::Selectable("None", noneSelected))
                {
                    m_Context.settings.environment.skyboxPath.clear();
                }
                if (noneSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }

                for (const std::filesystem::path &path : maps)
                {
                    const std::filesystem::path relative =
                        RendererSettingsPanelDetail::ToAssetsRelative(m_Context.assetsRootPath, path);
                    const std::string label = relative.generic_string();
                    const bool selected = relative == m_Context.settings.environment.skyboxPath;
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        m_Context.settings.environment.skyboxPath = relative;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::TextUnformatted("Input: one equirectangular HDR panorama from Assets/Textures/Env.");
            ImGui::TextUnformatted("Skybox EV adjusts background display brightness; IBL intensity is separate.");
            ImGui::TextUnformatted("IBL precompute will reuse this environment in a later phase.");
        }
    }
}