#pragma once

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class InspectorPanel final
    {
    public:
        explicit InspectorPanel(EditorContext &context);

        void Draw();

    private:
        EditorContext &m_Context;
    };
}