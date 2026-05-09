#pragma once

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class SceneViewPanel final
    {
    public:
        explicit SceneViewPanel(EditorContext &context);

        void Draw();

    private:
        EditorContext &m_Context;
    };
}