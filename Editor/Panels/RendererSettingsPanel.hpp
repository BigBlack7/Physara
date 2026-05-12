#pragma once

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Camera/EditorCamera.hpp>

namespace Physara::Editor
{
    class RendererSettingsPanel final
    {
    public:
        RendererSettingsPanel(EditorContext &context, EditorCamera &camera);

        void Draw();

    private:
        void DrawCameraSection();
        void DrawShadowSection();
        void DrawPostProcessSection();
        void DrawPipelineSection();
        void DrawDebugVisualizationSection();
        void DrawEnvironmentSection();

    private:
        EditorContext &m_Context;
        EditorCamera &m_Camera;
    };
}