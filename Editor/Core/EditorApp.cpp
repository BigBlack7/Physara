#include "EditorApp.hpp"
#include "EditorTheme.hpp"

#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Engine/Core/Log.hpp>
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
                             m_SceneViewPanel(m_Context, m_ShortcutRegistry),
                             m_ContentBrowserPanel(m_Context),
                             m_RendererSettingsPanel(m_Context),
                             m_HelpShortcutsPanel(m_Context, m_ShortcutRegistry)
    {
    }

    void EditorApp::Init(RHI::IImGuiBackend *backend)
    {
        m_Backend = backend;
        m_LayoutInitialized = false;
        m_DockspaceId = 0;
        m_Context.assetsRootPath = Physara::Platform::FileSystem::GetAssetsRootPath();
        m_Context.currentContentPath = m_Context.assetsRootPath;
        m_Context.settings.capture.outputDirectory = m_Context.assetsRootPath / "Gallery";

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

        HandleGlobalShortcuts();

        if (m_Context.ui.displayMode == EditorDisplayMode::Docked)
        {
            DrawMainDockSpace();
        }

        if (!m_LayoutInitialized)
        {
            InitDefaultLayout();
            m_LayoutInitialized = true;
        }

        DrawPanels();

        m_Backend->EndFrame();
        m_Backend->RenderDrawData();
    }

    void EditorApp::HandleGlobalShortcuts()
    {
        const bool textInputActive = ImGui::GetIO().WantTextInput;

        if (m_ShortcutRegistry.IsPressed("help.shortcuts"))
        {
            m_Context.ui.showHelpShortcuts = !m_Context.ui.showHelpShortcuts;
        }

        if (!textInputActive && m_ShortcutRegistry.IsPressed("viewport.presentation.toggle"))
        {
            m_Context.ui.displayMode = m_Context.ui.displayMode == EditorDisplayMode::Docked
                                           ? EditorDisplayMode::ViewportPresentation
                                           : EditorDisplayMode::Docked;
        }

        if (!textInputActive && m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation &&
            m_ShortcutRegistry.IsPressed("viewport.presentation.exit"))
        {
            m_Context.ui.displayMode = EditorDisplayMode::Docked;
        }

        if (!textInputActive && m_ShortcutRegistry.IsPressed("capture.current_view"))
        {
            RequestCapture();
        }
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
        if (m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation)
        {
            DrawPresentationPanels();
        }
        else
        {
            DrawDockedPanels();
        }

        m_HelpShortcutsPanel.Draw();
    }

    void EditorApp::DrawDockedPanels()
    {
        if (m_Context.ui.panels.hierarchy)
        {
            m_HierarchyPanel.Draw();
        }
        if (m_Context.ui.panels.rendererSettings)
        {
            m_RendererSettingsPanel.Draw();
        }
        m_SceneViewPanel.Draw();
        if (m_Context.ui.panels.inspector)
        {
            m_InspectorPanel.Draw();
        }
        if (m_Context.ui.panels.contentBrowser)
        {
            m_ContentBrowserPanel.Draw();
        }
        if (m_Context.ui.panels.log)
        {
            m_LogPanel.Draw();
        }
    }

    void EditorApp::DrawPresentationPanels()
    {
        m_SceneViewPanel.Draw();
    }

    void EditorApp::RequestCapture()
    {
        m_Context.settings.capture.captureRequested = true;
        PHYSARA_INFO("Capture requested. Renderer capture output will be connected in Phase 4.");
    }
}