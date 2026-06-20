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

        struct DebugViewOption
        {
            const char *label;
            int value;
        };

        std::vector<DebugViewOption> BuildDebugViewOptions(int renderPath)
        {
            std::vector<DebugViewOption> options{
                {"None", 0},
                {"Normals", 1},
                {"Depth", 2},
                {"Wireframe", 3},
                {"Shadow Map", 4},
                {"Shadow Cascades", 5}};
            if (renderPath >= 1)
            {
                options.push_back({"Light Clusters", 6});
            }
            if (renderPath == 2)
            {
                options.push_back({"GBuffer Base Color", 7});
                options.push_back({"GBuffer Normal", 8});
                options.push_back({"GBuffer Material", 9});
                options.push_back({"GBuffer Emissive", 10});
            }
            return options;
        }

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

        DrawPostProcessSection();
        DrawShadowSection();
        DrawEnvironmentSection();

        ImGui::End();
    }

    void RendererSettingsPanel::DrawPostProcessSection()
    {
        if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char *renderPathItems[] = {"Forward", "Forward+", "Deferred"};
            ImGui::Combo("Render Path", &m_Context.settings.postProcess.renderPathIndex, renderPathItems, IM_ARRAYSIZE(renderPathItems));
            const int renderPath = std::clamp(m_Context.settings.postProcess.renderPathIndex, 0, 2);
            const std::vector<RendererSettingsPanelDetail::DebugViewOption> debugOptions =
                RendererSettingsPanelDetail::BuildDebugViewOptions(renderPath);
            const auto selectedDebug = std::find_if(
                debugOptions.begin(),
                debugOptions.end(),
                [this](const RendererSettingsPanelDetail::DebugViewOption &option)
                {
                    return option.value == m_Context.settings.postProcess.debugViewIndex;
                });
            if (selectedDebug == debugOptions.end())
            {
                m_Context.settings.postProcess.debugViewIndex = 0;
            }
            const char *debugLabel = selectedDebug != debugOptions.end() ? selectedDebug->label : "None";
            if (ImGui::BeginCombo("Debug View", debugLabel))
            {
                for (const RendererSettingsPanelDetail::DebugViewOption &option : debugOptions)
                {
                    const bool selected = option.value == m_Context.settings.postProcess.debugViewIndex;
                    if (ImGui::Selectable(option.label, selected))
                    {
                        m_Context.settings.postProcess.debugViewIndex = option.value;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            if (m_Context.settings.postProcess.debugViewIndex == 4)
            {
                const int cascadeCount = std::clamp(m_Context.settings.shadow.cascadeCountIndex, 0, 2) + 2;
                m_Context.settings.postProcess.shadowMapCascadeIndex =
                    std::clamp(m_Context.settings.postProcess.shadowMapCascadeIndex, 0, cascadeCount - 1);
                const char *cascadeItems[] = {"Cascade 1", "Cascade 2", "Cascade 3", "Cascade 4"};
                ImGui::Combo(
                    "Shadow Map Cascade",
                    &m_Context.settings.postProcess.shadowMapCascadeIndex,
                    cascadeItems,
                    cascadeCount);
                ImGui::TextDisabled("Shadow map inspection locks Scene View camera navigation.");
            }
            const char *toneMappingItems[] = {"None", "ACES", "Reinhard", "Filmic", "Neutral"};
            ImGui::Combo("Tone Mapping", &m_Context.settings.postProcess.toneMappingModeIndex, toneMappingItems, IM_ARRAYSIZE(toneMappingItems));
            const char *aaItems[] = {"None", "MSAA", "FXAA Basic", "FXAA Quality", "SMAA Lite"};
            ImGui::Combo("Anti-Aliasing", &m_Context.settings.postProcess.antiAliasingModeIndex, aaItems, IM_ARRAYSIZE(aaItems));
            if (renderPath == 2 && m_Context.settings.postProcess.antiAliasingModeIndex == 1)
            {
                m_Context.settings.postProcess.antiAliasingModeIndex = 3;
            }
            if (m_Context.settings.postProcess.antiAliasingModeIndex == 1)
            {
                const char *msaaItems[] = {"2x", "4x", "8x"};
                ImGui::Combo("MSAA Samples", &m_Context.settings.postProcess.msaaSamplesIndex, msaaItems, IM_ARRAYSIZE(msaaItems));
            }
            if (renderPath == 2)
            {
                ImGui::TextDisabled("Deferred uses a single-sample GBuffer; post-process AA remains available.");
            }
            const bool postAA = m_Context.settings.postProcess.antiAliasingModeIndex >= 2;
            ImGui::BeginDisabled(!postAA);
            ImGui::SliderFloat("AA Subpixel", &m_Context.settings.postProcess.aaSubpixel, 0.f, 1.f, "%.2f");
            ImGui::SliderFloat("AA Edge", &m_Context.settings.postProcess.aaEdgeThreshold, 0.0312f, 0.333f, "%.4f");
            ImGui::SliderFloat("AA Edge Min", &m_Context.settings.postProcess.aaEdgeThresholdMin, 0.001f, 0.0833f, "%.4f");
            if (m_Context.settings.postProcess.antiAliasingModeIndex == 4)
            {
                ImGui::SliderFloat("Depth Sensitivity", &m_Context.settings.postProcess.aaDepthSensitivity, 0.f, 64.f, "%.1f");
            }
            ImGui::EndDisabled();
            ImGui::Checkbox("Bloom", &m_Context.settings.postProcess.bloomEnabled);
            ImGui::BeginDisabled(!m_Context.settings.postProcess.bloomEnabled);
            ImGui::SliderFloat("Threshold", &m_Context.settings.postProcess.bloomThreshold, 0.f, 10.f, "%.2f");
            ImGui::SliderFloat("Knee", &m_Context.settings.postProcess.bloomKnee, 0.f, 2.f, "%.2f");
            ImGui::SliderFloat("Intensity", &m_Context.settings.postProcess.bloomIntensity, 0.f, 4.f, "%.3f");
            ImGui::SliderFloat("Scatter", &m_Context.settings.postProcess.bloomScatter, 0.f, 1.f, "%.2f");
            ImGui::EndDisabled();
        }
    }

    void RendererSettingsPanel::DrawShadowSection()
    {
        if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Cascaded Shadows", &m_Context.settings.shadow.enabled);
            ImGui::BeginDisabled(!m_Context.settings.shadow.enabled);
            const char *filterItems[] = {"Hard", "PCF 3x3", "PCF 5x5", "Poisson 16", "PCSS"};
            ImGui::Combo("Filter", &m_Context.settings.shadow.filterIndex, filterItems, IM_ARRAYSIZE(filterItems));
            const char *resolutionItems[] = {"1024", "2048", "4096", "8192"};
            ImGui::Combo("Resolution / Cascade", &m_Context.settings.shadow.resolutionIndex, resolutionItems, IM_ARRAYSIZE(resolutionItems));
            const char *cascadeItems[] = {"2", "3", "4"};
            ImGui::Combo("Cascade Count", &m_Context.settings.shadow.cascadeCountIndex, cascadeItems, IM_ARRAYSIZE(cascadeItems));
            ImGui::SliderFloat("Shadow Distance", &m_Context.settings.shadow.maxDistanceMeters, 10.f, 2000.f, "%.0f m", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Split Lambda", &m_Context.settings.shadow.splitLambda, 0.f, 1.f, "%.2f");
            ImGui::SliderFloat("Cascade Blend", &m_Context.settings.shadow.transitionFraction, 0.f, 0.3f, "%.2f");
            ImGui::SliderFloat("Depth Bias", &m_Context.settings.shadow.depthBias, 0.f, 8.f, "%.2f");
            ImGui::SliderFloat("Slope Bias", &m_Context.settings.shadow.slopeBias, 0.f, 8.f, "%.2f");
            ImGui::SliderFloat("Normal Bias", &m_Context.settings.shadow.normalBiasTexels, 0.f, 8.f, "%.2f texels");
            ImGui::SliderFloat("Receiver Bias", &m_Context.settings.shadow.receiverBiasScale, 0.f, 4.f, "%.2f");
            ImGui::SliderFloat("Filter Radius", &m_Context.settings.shadow.filterRadiusTexels, 0.25f, 8.f, "%.2f texels");
            ImGui::BeginDisabled(m_Context.settings.shadow.filterIndex != 4);
            ImGui::SliderFloat("Light Size", &m_Context.settings.shadow.lightSizeTexels, 1.f, 128.f, "%.1f texels", ImGuiSliderFlags_Logarithmic);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        }
    }

    void RendererSettingsPanel::DrawEnvironmentSection()
    {
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Show Skybox", &m_Context.settings.environment.skyboxEnabled);
            ImGui::SliderFloat("Environment Intensity", &m_Context.settings.environment.skyboxIntensity, 0.f, 4.f, "%.2f");

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
        }
    }
}