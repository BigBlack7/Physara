#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/RenderView.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class Scene;
}

namespace Physara::Editor
{
    enum class GizmoOperation
    {
        None,
        Translate,
        Rotate,
        Scale
    };

    enum class GizmoSpace
    {
        Local,
        World
    };

    enum class EditorDisplayMode
    {
        Docked,
        ViewportPresentation
    };

    struct SceneViewState
    {
        float width{0.f};
        float height{0.f};
        bool hovered{false};
        bool focused{false};
        bool sizeChanged{false};
        bool flyCameraMode{false};
        bool playFlyMode{false};
        bool inputCaptured{false};
        Engine::RenderView lastRenderView{};
        Engine::FrameStatistics rendererStats{};
    };

    struct EditorFrameStatistics
    {
        float uiBuildCpuMs{0.f};
        float sceneRenderCpuMs{0.f};
        float uiRenderCpuMs{0.f};
        float frameCpuMs{0.f};
        RHI::ImGuiRenderStatistics imgui{};

        void Reset()
        {
            *this = {};
        }
    };

    struct PanelVisibilityState
    {
        bool hierarchy{true};
        bool rendererSettings{true};
        bool inspector{true};
        bool contentBrowser{true};
        bool log{true};
    };

    struct CaptureSettings
    {
        std::filesystem::path outputDirectory{};
        std::array<char, 64> fileNamePrefix{"Physara_Capture"};
        int fileFormatIndex{0};
        float resolutionScale{1.f};
        bool captureRequested{false};
    };

    struct EnvironmentSettings
    {
        std::filesystem::path skyboxPath{};
        float skyboxIntensity{1.f};
        bool skyboxEnabled{true};
    };

    struct PostProcessSettings
    {
        int toneMappingModeIndex{1};
        bool bloomEnabled{true};
        int antiAliasingModeIndex{3};
        int msaaSamplesIndex{1};
        int debugViewIndex{0};
        float bloomThreshold{1.f};
        float bloomKnee{0.5f};
        float bloomIntensity{0.12f};
        float bloomScatter{0.7f};
        float aaSubpixel{0.75f};
        float aaEdgeThreshold{0.125f};
        float aaEdgeThresholdMin{0.0312f};
        float aaDepthSensitivity{24.f};
    };

    struct ShadowSettings
    {
        bool enabled{true};
        int filterIndex{1};
        int resolutionIndex{1};
        int cascadeCountIndex{2};
        float maxDistanceMeters{250.f};
        float splitLambda{0.7f};
        float transitionFraction{0.1f};
        float depthBias{2.f};
        float slopeBias{2.f};
        float normalBiasTexels{1.5f};
        float receiverBiasScale{1.f};
        float filterRadiusTexels{1.5f};
        float lightSizeTexels{24.f};
    };

    struct EditorUIState
    {
        EditorDisplayMode displayMode{EditorDisplayMode::Docked};
        PanelVisibilityState panels{};
        bool showHelpShortcuts{false};
        bool showSceneViewInfo{true};
        bool showSceneViewPanelMenu{false};
        bool cleanSceneView{false};
    };

    struct EditorSettings
    {
        GizmoOperation gizmoOperation{GizmoOperation::Translate};
        GizmoSpace gizmoSpace{GizmoSpace::World};
        CaptureSettings capture{};
        EnvironmentSettings environment{};
        PostProcessSettings postProcess{};
        ShadowSettings shadow{};
    };

    struct EditorContext
    {
        Engine::Scene *activeScene{nullptr};     // non-owning
        Engine::EntityId selectedEntity{Engine::NullEntity};
        std::vector<Engine::EntityId> selectedEntities{};

        std::filesystem::path assetsRootPath{};
        std::filesystem::path currentContentPath{};
        std::filesystem::path currentScenePath{};

        SceneViewState sceneView{};
        EditorFrameStatistics frameStats{};
        EditorUIState ui{};
        EditorSettings settings{};
    };
}
