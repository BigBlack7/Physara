#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/RenderView.hpp>
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
        int antiAliasingModeIndex{2};
        int bloomModeIndex{1};
        int debugViewIndex{0};
        float bloomThreshold{1.f};
        float bloomKnee{0.5f};
        float bloomIntensity{0.12f};
        float bloomRadius{2.f};
        float aaSubpixel{0.75f};
        float aaEdgeThreshold{0.125f};
        float aaEdgeThresholdMin{0.0312f};
        float aaDepthSensitivity{24.f};
    };

    struct ShadowSettings
    {
        int algorithmIndex{1};
        int resolutionIndex{1};
        float depthBias{2.f};
        float slopeBias{2.f};
        float receiverBiasScale{1.f};
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
        EditorUIState ui{};
        EditorSettings settings{};
    };
}