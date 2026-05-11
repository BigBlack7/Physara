#pragma once

#include <array>
#include <filesystem>

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

    enum class RenderPipelineMode
    {
        Forward,
        ForwardPlus,
        Deferred
    };

    enum class DebugViewMode
    {
        None,
        Normals,
        Depth,
        Wireframe,
        LightComplexity,
        GBufferAlbedo,
        GBufferNormal,
        GBufferRoughness,
        GBufferMetallic,
        GBufferAO,
        GBufferEmissive,
        ShadowMap
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
        bool includeDebugView{false};
        bool captureRequested{false};
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
        RenderPipelineMode pipelineMode{RenderPipelineMode::Forward};
        DebugViewMode debugViewMode{DebugViewMode::None};
        CaptureSettings capture{};
    };

    struct EditorContext
    {
        Engine::Scene *activeScene{nullptr};     // non-owning
        Engine::EntityId selectedEntity{Engine::NullEntity};

        std::filesystem::path assetsRootPath{};
        std::filesystem::path currentContentPath{};
        std::filesystem::path currentScenePath{};

        SceneViewState sceneView{};
        EditorUIState ui{};
        EditorSettings settings{};
    };
}