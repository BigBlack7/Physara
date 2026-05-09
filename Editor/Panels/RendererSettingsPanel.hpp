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
        EditorContext &m_Context;
    };
}