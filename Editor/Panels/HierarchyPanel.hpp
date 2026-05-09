#pragma once

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class HierarchyPanel final
    {
    public:
        explicit HierarchyPanel(EditorContext &context);

        void Draw();

    private:
        EditorContext &m_Context;
    };
}