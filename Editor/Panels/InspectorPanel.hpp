#pragma once

#include <Editor/Core/EditorContext.hpp>
#include <Engine/Scene/Entity.hpp>

namespace Physara::Engine
{
    class AssetManager;
}

namespace Physara::Editor
{
    class InspectorPanel final
    {
    public:
        InspectorPanel(EditorContext &context, Engine::AssetManager &assetManager);

        void Draw();

    private:
        void DrawEntity(Engine::Entity entity);
        void DrawCameraCaptureSection(Engine::Entity entity);

    private:
        EditorContext &m_Context;
        Engine::AssetManager &m_AssetManager;
    };
}