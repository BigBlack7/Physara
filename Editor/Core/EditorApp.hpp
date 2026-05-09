#pragma once

#include <cstdint>

#include <Engine/RHI/Core/IImGuiBackend.hpp>

#include <Editor/Core/EditorContext.hpp>
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

        void Init(RHI::IImGuiBackend *backend);
        void OnUIRender();

        EditorContext &GetContext() { return m_Context; }
        const EditorContext &GetContext() const { return m_Context; }

    private:
        void InitDefaultLayout();
        void DrawMainDockSpace();
        void DrawPanels();

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        EditorContext m_Context{};
        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        SceneViewPanel m_SceneViewPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        LogPanel m_LogPanel;
        RendererSettingsPanel m_RendererSettingsPanel;

        bool m_LayoutInitialized{false};
        std::uint32_t m_DockspaceId{0};
    };
}