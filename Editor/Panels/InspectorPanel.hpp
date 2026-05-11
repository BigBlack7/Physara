#pragma once

#include <Editor/Core/EditorContext.hpp>
#include <Engine/Scene/Entity.hpp>

namespace Physara::Editor
{
    class InspectorPanel final
    {
    public:
        explicit InspectorPanel(EditorContext &context);

        void Draw();

    private:
        void DrawEntity(Engine::Entity entity);

    private:
        EditorContext &m_Context;
    };
}