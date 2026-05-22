#pragma once

#include <imgui/imgui.h>

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class Gizmo final
    {
    public:
        void Draw(EditorContext &context, const ImVec2 &viewportOrigin, float width, float height);
        [[nodiscard]] bool IsUsingOrHovered() const { return m_UsingOrHovered; }

    private:
        [[nodiscard]] bool CanDraw(const EditorContext &context, float width, float height) const;
    private:
        bool m_UsingOrHovered{false};
    };
}