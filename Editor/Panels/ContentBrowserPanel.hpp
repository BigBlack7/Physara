#pragma once

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class ContentBrowserPanel final
    {
    public:
        explicit ContentBrowserPanel(EditorContext &context);

        void Draw();

    private:
        bool CanNavigateToParent() const;
        void NavigateToParent();

    private:
        EditorContext &m_Context;
    };
}