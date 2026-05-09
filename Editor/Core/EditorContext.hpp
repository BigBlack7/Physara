#pragma once

#include <filesystem>

#include <entt/entt.hpp>

namespace Physara::Engine
{
    class Scene;
}

namespace Physara::Editor
{
    enum class GizmoOperation
    {
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

    struct SceneViewState
    {
        float width{0.f};
        float height{0.f};
        bool hovered{false};
        bool focused{false};
        bool sizeChanged{false};
    };

    struct EditorSettings
    {
        GizmoOperation gizmoOperation{GizmoOperation::Translate};
        GizmoSpace gizmoSpace{GizmoSpace::World};
        RenderPipelineMode pipelineMode{RenderPipelineMode::Forward};
        DebugViewMode debugViewMode{DebugViewMode::None};
    };

    struct EditorContext
    {
        Engine::Scene *activeScene{nullptr};     // non-owning
        entt::entity selectedEntity{entt::null}; // temporary id before Engine::Entity

        std::filesystem::path assetsRootPath{};
        std::filesystem::path currentContentPath{};

        SceneViewState sceneView{};
        EditorSettings settings{};
    };
}