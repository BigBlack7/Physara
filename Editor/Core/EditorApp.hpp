#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <Engine/RHI/Core/IImGuiBackend.hpp>

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/ShortcutRegistry.hpp>
#include <Editor/Panels/HelpShortcutsPanel.hpp>
#include <Editor/Panels/HierarchyPanel.hpp>
#include <Editor/Panels/InspectorPanel.hpp>
#include <Editor/Panels/SceneViewPanel.hpp>
#include <Editor/Panels/ContentBrowserPanel.hpp>
#include <Editor/Panels/LogPanel.hpp>
#include <Editor/Panels/RendererSettingsPanel.hpp>

namespace Physara::Editor
{
    class EditorApp
    {
    public:
        EditorApp();
        ~EditorApp();

        void Init(RHI::IImGuiBackend *backend);
        void Shutdown();
        void OnUIRender();

        EditorContext &GetContext() { return m_Context; }
        const EditorContext &GetContext() const { return m_Context; }

    private:
        void InitDefaultLayout();
        void HandleGlobalShortcuts();
        void DrawMainDockSpace();
        void DrawPanels();
        void DrawDockedPanels();
        void DrawPresentationPanels();
        void RequestCapture();
        void LoadSceneViewIcons();
        void DestroySceneViewIcons();
        void CreateDefaultScene();
        void DeleteSelectedEntity();

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        std::unique_ptr<Engine::Scene> m_EditorScene{};
        EditorContext m_Context{};
        ShortcutRegistry m_ShortcutRegistry{};
        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        SceneViewPanel m_SceneViewPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        LogPanel m_LogPanel;
        RendererSettingsPanel m_RendererSettingsPanel;
        HelpShortcutsPanel m_HelpShortcutsPanel;

        bool m_LayoutInitialized{false};
        std::uint32_t m_DockspaceId{0};
        std::vector<RHI::ImGuiTextureHandle> m_IconTextures{};
    };
}