#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <string>

#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/Renderer/Renderer.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Platform/Input/IInput.hpp>

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/IconManager.hpp>
#include <Editor/Core/ShortcutRegistry.hpp>
#include <Editor/Camera/EditorCamera.hpp>
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

        void Init(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Physara::Platform::IInput *input);
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
        void RenderSceneView();
        void RefreshSceneViewTexture();
        void RequestCapture();
        void ProcessCaptureRequests();
        Engine::CaptureDesc BuildCaptureDesc() const;
        void RequestSaveScene();
        void DrawSaveScenePopup();
        void SaveCurrentScene(const std::filesystem::path &path);
        std::filesystem::path BuildSceneSavePath(std::string name) const;
        void InitializeIcons();
        void CreateDefaultScene();
        void DeleteSelectedEntity();
        void ConnectSceneViewCameraInput();

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        RHI::RHIDevice *m_Device{nullptr};
        Physara::Platform::IInput *m_Input{nullptr};
        std::unique_ptr<Engine::Scene> m_EditorScene{};
        std::unique_ptr<Engine::Renderer> m_Renderer{};
        EditorContext m_Context{};
        ShortcutRegistry m_ShortcutRegistry{};
        IconManager m_IconManager{};
        Engine::AssetManager m_AssetManager{};
        EditorCamera m_EditorCamera{};
        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        SceneViewPanel m_SceneViewPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        LogPanel m_LogPanel;
        RendererSettingsPanel m_RendererSettingsPanel;
        HelpShortcutsPanel m_HelpShortcutsPanel;

        bool m_LayoutInitialized{false};
        bool m_OpenSaveScenePopup{false};
        Platform::CursorMode m_CurrentCursorMode{Platform::CursorMode::Normal};
        std::uint32_t m_DockspaceId{0};
        std::array<char, 128> m_SaveSceneName{};
    };
}