#include "EditorApp.hpp"
#include "EditorTheme.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
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
        constexpr const char *SceneSuffix = ".scene.json";

        std::string SceneNameFromPath(const std::filesystem::path &path)
        {
            std::string name = path.filename().string();
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });

            constexpr std::string_view suffix = SceneSuffix;
            if (lower.size() > suffix.size() && lower.ends_with(suffix))
            {
                name.resize(name.size() - suffix.size());
            }

            return name.empty() ? "Untitled" : name;
        }

    }

    EditorApp::EditorApp() : m_HierarchyPanel(m_Context),
                             m_InspectorPanel(m_Context),
                             m_SceneViewPanel(m_Context, m_ShortcutRegistry),
                             m_ContentBrowserPanel(m_Context, m_IconManager, m_AssetManager),
                             m_RendererSettingsPanel(m_Context, m_EditorCamera),
                             m_HelpShortcutsPanel(m_Context, m_ShortcutRegistry)
    {
    }

    EditorApp::~EditorApp() = default;

    void EditorApp::Init(RHI::RHIDevice *device, RHI::IImGuiBackend *backend)
    {
        m_Device = device;
        m_Backend = backend;
        m_Renderer = std::make_unique<Engine::Renderer>(m_Device);
        m_LayoutInitialized = false;
        m_DockspaceId = 0;
        m_Context.assetsRootPath = Physara::Platform::FileSystem::GetAssetsRootPath();
        m_AssetManager.SetAssetsRoot(m_Context.assetsRootPath);
        m_Context.currentContentPath = m_Context.assetsRootPath;
        m_Context.currentScenePath.clear();
        m_Context.settings.capture.outputDirectory = m_Context.assetsRootPath / "Gallery";
        std::snprintf(m_SaveSceneName.data(), m_SaveSceneName.size(), "%s", "Untitled");

        EditorTheme::Apply();
        CreateDefaultScene();
        InitializeIcons();
        ConnectSceneViewCameraInput();
    }

    void EditorApp::Shutdown()
    {
        m_SceneViewPanel.SetIconSet({});
        m_SceneViewPanel.SetPreviewTexture(0);
        m_IconManager.Shutdown();
        m_Renderer.reset();
        m_Context.activeScene = nullptr;
        m_Context.selectedEntity = Engine::NullEntity;
        m_EditorScene.reset();
        m_Device = nullptr;
        m_Backend = nullptr;
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
        RenderSceneView();

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
        DrawSaveScenePopup();

        if (m_Context.activeScene != nullptr)
        {
            m_Context.activeScene->UpdateTransforms();
        }

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

        if (!textInputActive && m_ShortcutRegistry.IsPressed("viewport.clean.toggle"))
        {
            m_Context.ui.cleanSceneView = !m_Context.ui.cleanSceneView;
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

        if (!textInputActive && m_ShortcutRegistry.IsPressed("scene.delete"))
        {
            DeleteSelectedEntity();
        }

        if (!textInputActive && m_ShortcutRegistry.IsPressed("scene.save"))
        {
            RequestSaveScene();
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

    void EditorApp::RenderSceneView()
    {
        if (m_Renderer == nullptr)
        {
            return;
        }

        if (m_Context.sceneView.width < 1.f || m_Context.sceneView.height < 1.f)
        {
            return;
        }

        const auto width = static_cast<std::uint32_t>(std::max(m_Context.sceneView.width, 1.f));
        const auto height = static_cast<std::uint32_t>(std::max(m_Context.sceneView.height, 1.f));
        m_Renderer->ResizeViewport(width, height);
        m_Renderer->RenderClear();
        RefreshSceneViewTexture();
    }

    void EditorApp::RefreshSceneViewTexture()
    {
        if (m_Backend == nullptr || m_Renderer == nullptr)
        {
            m_SceneViewPanel.SetPreviewTexture(0);
            return;
        }

        m_SceneViewPanel.SetPreviewTexture(m_Backend->GetTextureHandle(m_Renderer->GetSceneColorTexture()));
    }

    void EditorApp::RequestCapture()
    {
        m_Context.settings.capture.captureRequested = true;
        const Engine::RenderView captureView =
            m_EditorCamera.BuildCaptureView(m_Context.activeScene, m_Context.selectedEntity,
                                            m_EditorCamera.GetCaptureViewSource());
        PHYSARA_INFO("Capture requested. Viewport={}x{}, EV100={:.2f}. Renderer capture output will be connected in Phase 4.",
                     captureView.viewport.width, captureView.viewport.height, captureView.ev100);
    }

    void EditorApp::RequestSaveScene()
    {
        if (m_Context.activeScene == nullptr)
        {
            PHYSARA_WARN("No active scene to save.");
            return;
        }

        const std::string currentName = Internal::SceneNameFromPath(m_Context.currentScenePath);
        m_SaveSceneName.fill('\0');
        std::snprintf(m_SaveSceneName.data(), m_SaveSceneName.size(), "%s", currentName.c_str());
        m_OpenSaveScenePopup = true;
    }

    void EditorApp::DrawSaveScenePopup()
    {
        if (m_OpenSaveScenePopup)
        {
            ImGui::OpenPopup("Save Scene");
            m_OpenSaveScenePopup = false;
        }

        if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Save scene to Assets/Scenes");
            ImGui::InputText("Name", m_SaveSceneName.data(), m_SaveSceneName.size());
            ImGui::TextDisabled(".scene.json will be appended automatically");
            ImGui::Separator();

            const bool confirm = ImGui::Button("Save", ImVec2(96.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Enter);
            ImGui::SameLine();
            const bool cancel = ImGui::Button("Cancel", ImVec2(96.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

            if (confirm)
            {
                SaveCurrentScene(BuildSceneSavePath(m_SaveSceneName.data()));
                ImGui::CloseCurrentPopup();
            }
            else if (cancel)
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void EditorApp::SaveCurrentScene(const std::filesystem::path &path)
    {
        if (m_Context.activeScene == nullptr)
        {
            PHYSARA_WARN("No active scene to save.");
            return;
        }

        m_Context.activeScene->EnsureSceneCamera();
        if (Engine::SceneSerializer::Serialize(*m_Context.activeScene, path))
        {
            m_Context.currentScenePath = path;
            PHYSARA_INFO("Saved scene: {}", path.string());
        }
        else
        {
            PHYSARA_ERROR("Failed to save scene: {}", path.string());
        }
    }

    std::filesystem::path EditorApp::BuildSceneSavePath(std::string name) const
    {
        std::string sanitized;
        sanitized.reserve(name.size());
        for (char c : name)
        {
            const unsigned char value = static_cast<unsigned char>(c);
            if (std::isalnum(value) || c == '_' || c == '-' || c == ' ')
            {
                sanitized.push_back(c);
            }
            else if (c == '.')
            {
                sanitized.push_back(c);
            }
            else
            {
                sanitized.push_back('_');
            }
        }

        if (sanitized.empty())
        {
            sanitized = "Untitled";
        }

        std::string lower = sanitized;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        if (lower.ends_with(Internal::SceneSuffix))
        {
            sanitized.resize(sanitized.size() - std::string_view(Internal::SceneSuffix).size());
        }

        return m_Context.assetsRootPath / "Scenes" / (sanitized + Internal::SceneSuffix);
    }

    void EditorApp::InitializeIcons()
    {
        m_IconManager.Initialize(m_Backend, m_Context.assetsRootPath / "Icons");

        SceneViewIconSet icons{};
        icons.translate = m_IconManager.GetIcon(EditorIcon::Translate);
        icons.rotate = m_IconManager.GetIcon(EditorIcon::Rotate);
        icons.scale = m_IconManager.GetIcon(EditorIcon::Scale);
        icons.panel = m_IconManager.GetIcon(EditorIcon::Panel);
        icons.shortcut = m_IconManager.GetIcon(EditorIcon::Shortcut);
        icons.info = m_IconManager.GetIcon(EditorIcon::Info);

        m_SceneViewPanel.SetIconSet(icons);
    }

    void EditorApp::CreateDefaultScene()
    {
        m_EditorScene = std::make_unique<Engine::Scene>();
        Engine::Entity entity = m_EditorScene->EnsureSceneCamera();
        entity.GetComponent<Engine::TransformComponent>().SetLocalPosition({0.f, 1.6f, 5.f});
        m_Context.activeScene = m_EditorScene.get();
        m_Context.selectedEntity = entity.GetHandle();
    }

    void EditorApp::DeleteSelectedEntity()
    {
        if (m_Context.activeScene == nullptr || !m_Context.activeScene->IsValid(m_Context.selectedEntity))
        {
            m_Context.selectedEntity = Engine::NullEntity;
            return;
        }

        if (m_Context.activeScene->IsSceneCamera(m_Context.selectedEntity))
        {
            PHYSARA_INFO("Scene Camera is global and cannot be deleted.");
            return;
        }

        m_Context.activeScene->DestroyEntity(m_Context.selectedEntity);
        m_Context.selectedEntity = Engine::NullEntity;
    }

    void EditorApp::ConnectSceneViewCameraInput()
    {
        m_SceneViewPanel.SetViewportResizeCallback([this](std::uint32_t width, std::uint32_t height)
                                                   {
                                                       m_EditorCamera.SetViewportSize(width, height);
                                                       if (m_Renderer != nullptr)
                                                       {
                                                           m_Renderer->ResizeViewport(width, height);
                                                           m_Renderer->RenderClear();
                                                           RefreshSceneViewTexture();
                                                       }
                                                   });

        m_SceneViewPanel.SetInputForwardCallback([this](const EditorCameraInputFrame &input)
                                                 {
                                                     const float deltaTime = std::max(ImGui::GetIO().DeltaTime, 0.f);
                                                     m_EditorCamera.Update(input, deltaTime);
                                                 });
    }
}