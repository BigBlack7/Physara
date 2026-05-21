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
            const char *debugItems[] = {"None", "Normals", "Depth"};
            ImGui::Combo("Debug View", &m_Context.settings.postProcess.debugViewIndex, debugItems, IM_ARRAYSIZE(debugItems));
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

    void RendererSettingsPanel::DrawShadowSection()
    {
        if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char *algorithmItems[] = {"None", "Single Map PCF 3x3"};
            ImGui::Combo("Algorithm", &m_Context.settings.shadow.algorithmIndex, algorithmItems, IM_ARRAYSIZE(algorithmItems));

            const bool shadowEnabled = m_Context.settings.shadow.algorithmIndex != 0;
            ImGui::BeginDisabled(!shadowEnabled);
            const char *resolutionItems[] = {"1024", "2048", "4096"};
            ImGui::Combo("Resolution", &m_Context.settings.shadow.resolutionIndex, resolutionItems, IM_ARRAYSIZE(resolutionItems));
            ImGui::SliderFloat("Depth Bias", &m_Context.settings.shadow.depthBias, 0.f, 8.f, "%.2f");
            ImGui::SliderFloat("Slope Bias", &m_Context.settings.shadow.slopeBias, 0.f, 8.f, "%.2f");
            ImGui::SliderFloat("Receiver Bias", &m_Context.settings.shadow.receiverBiasScale, 0.f, 4.f, "%.2f");
            ImGui::EndDisabled();
        }
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
        }
    }
}