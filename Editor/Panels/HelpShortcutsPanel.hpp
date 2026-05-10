#pragma once

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/ShortcutRegistry.hpp>

namespace Physara::Editor
{
    class HelpShortcutsPanel final
    {
    public:
        HelpShortcutsPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry);

        void Draw();

    private:
        EditorContext &m_Context;
        const ShortcutRegistry &m_ShortcutRegistry;
    };
}