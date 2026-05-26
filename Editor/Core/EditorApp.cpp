#include "EditorApp.hpp"
#include "EditorTheme.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/ImGuizmo.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/BuiltinPrimitives.hpp>
#include <Engine/Resource/Loaders/GLTFLoader.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Editor/Camera/EditorCamera.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Editor
{
    namespace EditorAppDetail
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

        std::string SanitizeFileStem(std::string_view value, bool allowDot = false, std::string_view fallback = "Physara_Capture")
        {
            std::string sanitized;
            sanitized.reserve(value.size());

            for (char c : value)
            {
                const unsigned char ch = static_cast<unsigned char>(c);
                if (std::isalnum(ch) || c == '_' || c == '-' || c == ' ' || (allowDot && c == '.'))
                {
                    sanitized.push_back(c);
                }
                else
                {
                    sanitized.push_back('_');
                }
            }

            return sanitized.empty() ? std::string(fallback) : sanitized;
        }

        std::string TimestampForFileName()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

            std::tm localTime{};
#if defined(_WIN32)
            localtime_s(&localTime, &time);
#else
            localtime_r(&time, &localTime);
#endif

            std::ostringstream stream;
            stream << std::put_time(&localTime, "%Y%m%d_%H%M%S") << '_' << std::setw(3) << std::setfill('0') << milliseconds;
            return stream.str();
        }

        Engine::CaptureFormat CaptureFormatFromIndex(int index)
        {
            switch (index)
            {
            case 1:
                return Engine::CaptureFormat::JPG;
            case 2:
                return Engine::CaptureFormat::EXR;
            case 0:
            default:
                return Engine::CaptureFormat::PNG;
            }
        }

        void ExpandBounds(glm::vec3 &minBounds, glm::vec3 &maxBounds, const glm::vec3 &point)
        {
            minBounds.x = std::min(minBounds.x, point.x);
            minBounds.y = std::min(minBounds.y, point.y);
            minBounds.z = std::min(minBounds.z, point.z);
            maxBounds.x = std::max(maxBounds.x, point.x);
            maxBounds.y = std::max(maxBounds.y, point.y);
            maxBounds.z = std::max(maxBounds.z, point.z);
        }

        float EnvironmentIntensityToEV(float intensity, float cameraEV100)
        {
            constexpr float blackEV = -32.f;
            constexpr float nativeIntensity = 1.f;
            constexpr float maxEditorIntensity = 4.f;
            constexpr float maxBoostEV = 8.f;
            constexpr float boostCurve = 1.45f;
            constexpr float exposureCalibration = 1.2f;
            const float neutralEV = std::max(cameraEV100, 0.f) + std::log2(exposureCalibration);

            if (!std::isfinite(intensity) || intensity <= 0.f)
            {
                return blackEV;
            }
            if (intensity < nativeIntensity)
            {
                return neutralEV + std::log2(std::max(intensity, std::exp2(blackEV)));
            }

            const float t = std::clamp((intensity - nativeIntensity) / (maxEditorIntensity - nativeIntensity), 0.f, 1.f);
            return neutralEV + maxBoostEV * std::pow(t, boostCurve);
        }

    }

    EditorApp::EditorApp() : m_HierarchyPanel(m_Context),
                             m_InspectorPanel(m_Context, m_AssetManager),
                             m_SceneViewPanel(m_Context, m_ShortcutRegistry),
                             m_ContentBrowserPanel(m_Context, m_IconManager, m_AssetManager),
                             m_RendererSettingsPanel(m_Context),
                             m_HelpShortcutsPanel(m_Context, m_ShortcutRegistry)
    {
    }

    EditorApp::~EditorApp() = default;

    void EditorApp::Init(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Platform::IInput *input)
    {
        m_Device = device;
        m_Backend = backend;
        m_Input = input;
        m_Renderer = std::make_unique<Engine::Renderer>(m_Device);
        m_Renderer->SetAssetManager(&m_AssetManager);
        m_Renderer->SetClearColor({0.16f, 0.22f, 0.2f, 1.f});
        m_LayoutInitialized = false;
        m_DockspaceId = 0;
        m_Context.assetsRootPath = Physara::Platform::FileSystem::GetAssetsRootPath();
        m_AssetManager.SetAssetsRoot(m_Context.assetsRootPath);
        m_Context.currentContentPath = m_Context.assetsRootPath;
        m_Context.currentScenePath.clear();
        m_Context.settings.capture.outputDirectory = m_Context.assetsRootPath / "Gallery";
        std::snprintf(m_SaveSceneName.data(), m_SaveSceneName.size(), "%s", "Untitled");
        Engine::RegisterBuiltinPrimitives(m_AssetManager);

        EditorTheme::Apply();
        CreateDefaultScene();
        InitializeIcons();
        ConnectSceneViewCameraInput();
    }

    void EditorApp::Shutdown()
    {
        if (m_Input != nullptr)
        {
            m_Input->SetCursorMode(Platform::CursorMode::Normal);
            m_CurrentCursorMode = Platform::CursorMode::Normal;
        }

        m_SceneViewPanel.SetIconSet({});
        m_SceneViewPanel.SetPreviewTexture(0);
        m_IconManager.Shutdown();
        m_Renderer.reset();
        m_Context.activeScene = nullptr;
        m_Context.selectedEntity = Engine::NullEntity;
        m_Context.selectedEntities.clear();
        m_EditorScene.reset();
        m_Device = nullptr;
        m_Backend = nullptr;
        m_Input = nullptr;
    }

    void EditorApp::OnUIRender()
    {
        if (m_Backend == nullptr)
        {
            return;
        }

        m_Backend->BeginFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

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
        ProcessCaptureRequests();
        DrawSaveScenePopup();

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

        if (!textInputActive && !m_Context.sceneView.inputCaptured)
        {
            if (m_ShortcutRegistry.IsPressed("gizmo.translate"))
            {
                m_Context.settings.gizmoOperation = GizmoOperation::Translate;
            }
            if (m_ShortcutRegistry.IsPressed("gizmo.rotate"))
            {
                m_Context.settings.gizmoOperation = GizmoOperation::Rotate;
            }
            if (m_ShortcutRegistry.IsPressed("gizmo.scale"))
            {
                m_Context.settings.gizmoOperation = GizmoOperation::Scale;
            }
            if (m_ShortcutRegistry.IsPressed("gizmo.space.toggle"))
            {
                m_Context.settings.gizmoSpace = m_Context.settings.gizmoSpace == GizmoSpace::Local
                                                    ? GizmoSpace::World
                                                    : GizmoSpace::Local;
            }
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
                                : ImGui::GetID(EditorAppDetail::DockspaceName);

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

        ImGui::DockBuilderDockWindow(EditorAppDetail::HierarchyName, left);
        ImGui::DockBuilderDockWindow(EditorAppDetail::RendererSettingsName, left);
        ImGui::DockBuilderDockWindow(EditorAppDetail::SceneViewName, center);
        ImGui::DockBuilderDockWindow(EditorAppDetail::InspectorName, right);
        ImGui::DockBuilderDockWindow(EditorAppDetail::ContentBrowserName, bottom);
        ImGui::DockBuilderDockWindow(EditorAppDetail::LogName, bottom);

        ImGui::DockBuilderFinish(dockspace);
    }

    void EditorApp::DrawMainDockSpace()
    {
        const ImGuiID dockspaceId = ImGui::GetID(EditorAppDetail::DockspaceName);
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
            DrawPanelDisabledIfCaptured([this]()
                                        { m_HierarchyPanel.Draw(); });
        }
        if (m_Context.ui.panels.rendererSettings)
        {
            DrawPanelDisabledIfCaptured([this]()
                                        { m_RendererSettingsPanel.Draw(); });
        }
        m_SceneViewPanel.Draw();
        if (m_Context.ui.panels.inspector)
        {
            DrawPanelDisabledIfCaptured([this]()
                                        { m_InspectorPanel.Draw(); });
        }
        if (m_Context.ui.panels.contentBrowser)
        {
            DrawPanelDisabledIfCaptured([this]()
                                        { m_ContentBrowserPanel.Draw(); });
        }
        if (m_Context.ui.panels.log)
        {
            DrawPanelDisabledIfCaptured([this]()
                                        { m_LogPanel.Draw(); });
        }
    }

    void EditorApp::DrawPresentationPanels()
    {
        m_SceneViewPanel.Draw();
    }

    void EditorApp::DrawPanelDisabledIfCaptured(const std::function<void()> &drawFn)
    {
        const bool disablePanelInput = m_Context.sceneView.inputCaptured;
        if (disablePanelInput)
        {
            ImGui::BeginDisabled();
        }

        drawFn();

        if (disablePanelInput)
        {
            ImGui::EndDisabled();
        }
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
        const bool sceneTransformsUpdated = m_Context.activeScene != nullptr;
        if (sceneTransformsUpdated)
        {
            m_Context.activeScene->EnsureSceneCamera();
            m_Context.activeScene->UpdateTransforms();
        }
        Engine::RenderView view = m_EditorCamera.BuildRenderView(m_Context.activeScene);
        view.viewport.width = width;
        view.viewport.height = height;
        m_Context.sceneView.lastRenderView = view;
        const std::filesystem::path environmentPath =
            !m_Context.settings.environment.skyboxPath.empty()
                ? m_Context.assetsRootPath / m_Context.settings.environment.skyboxPath
                : std::filesystem::path{};
        m_Renderer->SetSkyboxEnabled(m_Context.settings.environment.skyboxEnabled);
        m_Renderer->SetSkyboxExposureCompensation(EditorAppDetail::EnvironmentIntensityToEV(m_Context.settings.environment.skyboxIntensity, view.ev100));
        m_Renderer->SetEnvironmentMapPath(environmentPath);
        Engine::PostProcessSettings postProcessSettings{};
        postProcessSettings.toneMappingMode = static_cast<Engine::ToneMappingMode>(
            std::clamp(m_Context.settings.postProcess.toneMappingModeIndex, 0, 4));
        postProcessSettings.bloomEnabled = m_Context.settings.postProcess.bloomEnabled;
        const int aaIndex = std::clamp(m_Context.settings.postProcess.antiAliasingModeIndex, 0, 4);
        postProcessSettings.antiAliasingMode = aaIndex <= 1
                                                   ? Engine::AntiAliasingMode::None
                                                   : static_cast<Engine::AntiAliasingMode>(aaIndex - 1);
        postProcessSettings.bloomMode = static_cast<Engine::BloomMode>(
            std::clamp(m_Context.settings.postProcess.bloomModeIndex, 0, 2));
        postProcessSettings.debugView = static_cast<Engine::DebugViewMode>(std::clamp(m_Context.settings.postProcess.debugViewIndex, 0, 2));
        if (m_Context.activeScene != nullptr)
        {
            Engine::Entity sceneCamera = m_Context.activeScene->GetSceneCameraEntity();
            if (sceneCamera.HasComponent<Engine::CameraComponent>())
            {
                const Engine::CameraComponent &camera = sceneCamera.GetComponent<Engine::CameraComponent>();
                postProcessSettings.exposureMode = Engine::ExposureMode::Manual;
                postProcessSettings.exposureCompensationEV = camera.exposureCompensationEV;
            }
        }
        postProcessSettings.bloomThreshold = m_Context.settings.postProcess.bloomThreshold;
        postProcessSettings.bloomKnee = m_Context.settings.postProcess.bloomKnee;
        postProcessSettings.bloomIntensity = m_Context.settings.postProcess.bloomIntensity;
        postProcessSettings.bloomRadius = m_Context.settings.postProcess.bloomRadius;
        postProcessSettings.aaSubpixel = m_Context.settings.postProcess.aaSubpixel;
        postProcessSettings.aaEdgeThreshold = m_Context.settings.postProcess.aaEdgeThreshold;
        postProcessSettings.aaEdgeThresholdMin = m_Context.settings.postProcess.aaEdgeThresholdMin;
        postProcessSettings.aaDepthSensitivity = m_Context.settings.postProcess.aaDepthSensitivity;
        m_Renderer->SetPostProcessSettings(postProcessSettings);
        const std::uint32_t msaaSamples[] = {2u, 4u, 8u};
        m_Renderer->SetMSAASamples(aaIndex == 1 ? msaaSamples[std::clamp(m_Context.settings.postProcess.msaaSamplesIndex, 0, 2)] : 1u);
        Engine::ShadowSettings shadowSettings{};
        const int shadowAlgorithmIndex = std::clamp(m_Context.settings.shadow.algorithmIndex, 0, 5);
        shadowSettings.algorithm = static_cast<Engine::ShadowAlgorithm>(shadowAlgorithmIndex);
        const std::uint32_t shadowResolutions[] = {1024u, 2048u, 4096u, 8192u};
        const int shadowResolutionIndex = std::clamp(m_Context.settings.shadow.resolutionIndex, 0, 3);
        shadowSettings.resolution = shadowResolutions[shadowResolutionIndex];
        shadowSettings.depthBias = m_Context.settings.shadow.depthBias;
        shadowSettings.slopeBias = m_Context.settings.shadow.slopeBias;
        shadowSettings.receiverBiasScale = m_Context.settings.shadow.receiverBiasScale;
        shadowSettings.filterRadiusTexels = m_Context.settings.shadow.filterRadiusTexels;
        shadowSettings.lightSizeTexels = m_Context.settings.shadow.lightSizeTexels;
        m_Renderer->SetShadowSettings(shadowSettings);
        if (m_Context.activeScene != nullptr)
        {
            m_Renderer->RenderScene(*m_Context.activeScene, view, std::max(ImGui::GetIO().DeltaTime, 0.f), sceneTransformsUpdated);
        }
        else
        {
            m_Renderer->Render(view, std::max(ImGui::GetIO().DeltaTime, 0.f));
        }
        m_Context.sceneView.rendererStats = m_Renderer->GetFrameData().stats;
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
        if (m_Renderer == nullptr)
        {
            PHYSARA_WARN("Capture skipped because renderer is not initialized.");
            return;
        }

        Engine::CaptureDesc desc = BuildCaptureDesc();
        m_Renderer->RequestCapture(desc);
        PHYSARA_INFO("Capture queued: {}", desc.outputPath.string());
    }

    void EditorApp::ProcessCaptureRequests()
    {
        if (!m_Context.settings.capture.captureRequested)
        {
            return;
        }

        m_Context.settings.capture.captureRequested = false;
        RequestCapture();
    }

    Engine::CaptureDesc EditorApp::BuildCaptureDesc() const
    {
        const Engine::CaptureFormat format =
            EditorAppDetail::CaptureFormatFromIndex(m_Context.settings.capture.fileFormatIndex);
        const std::string prefix =
            EditorAppDetail::SanitizeFileStem(m_Context.settings.capture.fileNamePrefix.data());
        const std::filesystem::path directory =
            m_Context.settings.capture.outputDirectory.empty()
                ? m_Context.assetsRootPath / "Gallery"
                : m_Context.settings.capture.outputDirectory;

        Engine::CaptureDesc desc{};
        desc.format = format;
        desc.outputPath = directory / (prefix + "_" + EditorAppDetail::TimestampForFileName() + std::string(Engine::GetCaptureFormatExtension(format)));
        desc.resolutionScale = m_Context.settings.capture.resolutionScale;
        desc.jpgQuality = 95;
        return desc;
    }

    void EditorApp::RequestSaveScene()
    {
        if (m_Context.activeScene == nullptr)
        {
            PHYSARA_WARN("No active scene to save.");
            return;
        }

        const std::string currentName = EditorAppDetail::SceneNameFromPath(m_Context.currentScenePath);
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
        std::string sanitized = EditorAppDetail::SanitizeFileStem(name, true, "Untitled");

        std::string lower = sanitized;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        if (lower.ends_with(EditorAppDetail::SceneSuffix))
        {
            sanitized.resize(sanitized.size() - std::string_view(EditorAppDetail::SceneSuffix).size());
        }

        return m_Context.assetsRootPath / "Scenes" / (sanitized + EditorAppDetail::SceneSuffix);
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
        icons.lightBillboard = m_IconManager.GetIcon(EditorIcon::Billboard);

        m_SceneViewPanel.SetIconSet(icons);
    }

    void EditorApp::CreateDefaultScene()
    {
        m_EditorScene = std::make_unique<Engine::Scene>();
        m_Context.activeScene = m_EditorScene.get();

        const std::filesystem::path defaultScenePath = m_Context.assetsRootPath / "Scenes" / "default.scene.json";
        bool loadedDefaultScene = false;
        if (std::filesystem::exists(defaultScenePath))
        {
            loadedDefaultScene = Engine::SceneSerializer::Deserialize(*m_EditorScene, defaultScenePath, &m_AssetManager);
            if (loadedDefaultScene)
            {
                m_Context.currentScenePath = defaultScenePath;
                PHYSARA_INFO("Loaded default scene: {}", defaultScenePath.string());
            }
            else
            {
                PHYSARA_ERROR("Failed to load default scene: {}", defaultScenePath.string());
            }
        }

        if (!loadedDefaultScene)
        {
            Engine::Entity importRoot = Engine::GLTFLoader::LoadToScene(
                *m_EditorScene,
                m_Context.assetsRootPath / "Models" / "su7_ultra" / "su7_ultra.gltf",
                &m_AssetManager);
            if (importRoot)
            {
                PHYSARA_INFO("Fallback startup GLTF loaded.");
            }
        }

        Engine::Entity entity = m_EditorScene->EnsureSceneCamera();
        entity.GetComponent<Engine::TransformComponent>().SetLocalPosition({0.f, 1.6f, 5.f});
        FrameEditorCameraToScene();
        m_EditorCamera.SyncToSceneCamera(m_EditorScene.get());
        m_Context.selectedEntity = entity.GetHandle();
        m_Context.selectedEntities.clear();
        m_Context.selectedEntities.push_back(entity.GetHandle());
    }

    void EditorApp::FrameEditorCameraToScene()
    {
        if (m_Context.activeScene == nullptr)
        {
            m_EditorCamera.SetPosition({0.f, 2.f, 6.f});
            m_EditorCamera.SetYawPitchDegrees(-90.f, -12.f);
            return;
        }

        m_Context.activeScene->UpdateTransforms();
        auto &registry = m_Context.activeScene->GetRegistry();
        auto view = registry.view<Engine::MeshComponent, Engine::TransformComponent>();

        bool hasBounds = false;
        glm::vec3 minBounds(0.f);
        glm::vec3 maxBounds(0.f);

        view.each([&hasBounds, &minBounds, &maxBounds](Engine::EntityId, const Engine::MeshComponent &mesh, const Engine::TransformComponent &transform)
        {
            if (!mesh.visible || !mesh.localBounds.valid)
            {
                return;
            }

            const glm::vec3 localMin = mesh.localBounds.min;
            const glm::vec3 localMax = mesh.localBounds.max;
            const glm::mat4 world = transform.GetWorldMatrix();
            const glm::vec3 corners[8]{
                {localMin.x, localMin.y, localMin.z},
                {localMax.x, localMin.y, localMin.z},
                {localMin.x, localMax.y, localMin.z},
                {localMax.x, localMax.y, localMin.z},
                {localMin.x, localMin.y, localMax.z},
                {localMax.x, localMin.y, localMax.z},
                {localMin.x, localMax.y, localMax.z},
                {localMax.x, localMax.y, localMax.z}};

            for (const glm::vec3 &corner : corners)
            {
                const glm::vec3 worldCorner = glm::vec3(world * glm::vec4(corner, 1.f));
                if (!hasBounds)
                {
                    minBounds = worldCorner;
                    maxBounds = worldCorner;
                    hasBounds = true;
                }
                else
                {
                    EditorAppDetail::ExpandBounds(minBounds, maxBounds, worldCorner);
                }
            }
        });

        if (!hasBounds)
        {
            m_EditorCamera.SetPosition({0.f, 2.f, 6.f});
            m_EditorCamera.SetYawPitchDegrees(-90.f, -12.f);
            return;
        }

        const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
        const glm::vec3 extent = maxBounds - minBounds;
        const float radius = std::max(std::max(extent.x, extent.y), extent.z) * 0.5f;
        const float distance = std::max(radius * 3.2f, 4.f);
        const glm::vec3 direction = glm::normalize(glm::vec3(0.55f, 0.28f, 1.f));
        const glm::vec3 position = center + direction * distance;
        const glm::vec3 toCenter = glm::normalize(center - position);
        const float yaw = glm::degrees(std::atan2(toCenter.z, toCenter.x));
        const float pitch = glm::degrees(std::asin(toCenter.y));

        m_EditorCamera.SetPosition(position);
        m_EditorCamera.SetYawPitchDegrees(yaw, pitch);
    }

    void EditorApp::DeleteSelectedEntity()
    {
        if (m_Context.activeScene == nullptr || !m_Context.activeScene->IsValid(m_Context.selectedEntity))
        {
            m_Context.selectedEntity = Engine::NullEntity;
            m_Context.selectedEntities.clear();
            return;
        }

        if (m_Context.activeScene->IsSceneCamera(m_Context.selectedEntity))
        {
            PHYSARA_INFO("The global camera cannot be deleted.");
            return;
        }

        m_Context.activeScene->DestroyEntity(m_Context.selectedEntity);
        m_Context.selectedEntity = Engine::NullEntity;
        m_Context.selectedEntities.clear();
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
                                                       } });

        m_SceneViewPanel.SetInputForwardCallback([this](const EditorCameraInputFrame &input)
                                                 {
                                                     const float deltaTime = std::max(ImGui::GetIO().DeltaTime, 0.f);
                                                     EditorCameraInputFrame cameraInput = input;
                                                     const bool enteringCapture = input.hovered &&
                                                                                  (input.rightMouseDown || input.gravePressed) &&
                                                                                  m_CurrentCursorMode == Platform::CursorMode::Normal;
                                                     if (enteringCapture)
                                                     {
                                                         cameraInput.mouseDeltaX = 0.f;
                                                         cameraInput.mouseDeltaY = 0.f;
                                                     }
                                                     else if (m_Input != nullptr && m_CurrentCursorMode == Platform::CursorMode::Locked)
                                                     {
                                                         const glm::vec2 mouseDelta = m_Input->GetMouseDelta();
                                                         cameraInput.mouseDeltaX = mouseDelta.x;
                                                         cameraInput.mouseDeltaY = mouseDelta.y;
                                                     }

                                                     m_EditorCamera.SyncFromSceneCamera(m_Context.activeScene);
                                                     m_EditorCamera.Update(cameraInput, deltaTime);
                                                     m_EditorCamera.SyncToSceneCamera(m_Context.activeScene);
                                                     m_Context.sceneView.flyCameraMode = m_EditorCamera.GetMode() != EditorCameraMode::Orbit;
                                                     m_Context.sceneView.playFlyMode = m_EditorCamera.IsPlayFlyModeActive();
                                                     if (m_Input != nullptr)
                                                     {
                                                         const Platform::CursorMode desiredCursorMode =
                                                             m_EditorCamera.WantsLockedCursor()
                                                                 ? Platform::CursorMode::Locked
                                                                 : Platform::CursorMode::Normal;
                                                         if (m_CurrentCursorMode != desiredCursorMode)
                                                         {
                                                             m_Input->SetCursorMode(desiredCursorMode);
                                                             m_CurrentCursorMode = desiredCursorMode;
                                                         }
                                                     } });
    }
}