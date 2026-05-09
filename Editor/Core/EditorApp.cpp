#include "EditorApp.hpp"
#include "EditorTheme.hpp"

#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Editor
{

    namespace Internal
    {
        constexpr const char *DockspaceName = "MainDockSpace";
        constexpr const char *SceneViewName = "Scene View";
        constexpr const char *HierarchyName = "Hierarchy";
        constexpr const char *RendererSettingsName = "Renderer Settings";
        constexpr const char *InspectorName = "Inspector";
        constexpr const char *ContentBrowserName = "Content Browser";
        constexpr const char *LogName = "Log";
    }

    EditorApp::EditorApp() : m_HierarchyPanel(m_Context),
                             m_InspectorPanel(m_Context),
                             m_SceneViewPanel(m_Context),
                             m_ContentBrowserPanel(m_Context),
                             m_RendererSettingsPanel(m_Context)
    {
    }

    void EditorApp::Init(RHI::IImGuiBackend *backend)
    {
        m_Backend = backend;
        m_LayoutInitialized = false;
        m_DockspaceId = 0;
        m_Context.assetsRootPath = Physara::Platform::FileSystem::GetAssetsRootPath();
        m_Context.currentContentPath = m_Context.assetsRootPath;

        EditorTheme::Apply();
    }

    void EditorApp::OnUIRender()
    {
        if (m_Backend == nullptr)
        {
            return;
        }

        m_Backend->BeginFrame();
        ImGui::NewFrame();

        DrawMainDockSpace();

        if (!m_LayoutInitialized)
        {
            InitDefaultLayout();
            m_LayoutInitialized = true;
        }

        DrawPanels();

        m_Backend->EndFrame();
        m_Backend->RenderDrawData();
    }

    void EditorApp::InitDefaultLayout()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.IniFilename != nullptr && std::filesystem::exists(io.IniFilename))
        {
            return;
        }

        ImGuiID dockspace = m_DockspaceId != 0
                                ? static_cast<ImGuiID>(m_DockspaceId)
                                : ImGui::GetID(Internal::DockspaceName);

        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->Size);

        ImGuiID center = dockspace;
        ImGuiID left = 0;
        ImGuiID right = 0;
        ImGuiID bottom = 0;

        const float leftRatio = 0.18f;
        const float rightRatio = 0.22f;
        const float bottomRatio = 0.26f;

        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, leftRatio, &left, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, rightRatio, &right, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, bottomRatio, &bottom, &center);

        ImGui::DockBuilderDockWindow(Internal::HierarchyName, left);
        ImGui::DockBuilderDockWindow(Internal::RendererSettingsName, left);
        ImGui::DockBuilderDockWindow(Internal::SceneViewName, center);
        ImGui::DockBuilderDockWindow(Internal::InspectorName, right);
        ImGui::DockBuilderDockWindow(Internal::ContentBrowserName, bottom);
        ImGui::DockBuilderDockWindow(Internal::LogName, bottom);

        ImGui::DockBuilderFinish(dockspace);
    }

    void EditorApp::DrawMainDockSpace()
    {
        const ImGuiID dockspaceId = ImGui::GetID(Internal::DockspaceName);
        ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
        m_DockspaceId = static_cast<std::uint32_t>(dockspaceId);
    }

    void EditorApp::DrawPanels()
    {
        m_HierarchyPanel.Draw();
        m_RendererSettingsPanel.Draw();
        m_SceneViewPanel.Draw();
        m_InspectorPanel.Draw();
        m_ContentBrowserPanel.Draw();
        m_LogPanel.Draw();
    }
}