#pragma once

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class RendererSettingsPanel final
    {
    public:
        explicit RendererSettingsPanel(EditorContext &context);

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
    };
}