#pragma once

#include <array>

#include <Editor/Core/EditorContext.hpp>
#include <Engine/Scene/Entity.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Editor
{
    class HierarchyPanel final
    {
    public:
        explicit HierarchyPanel(EditorContext &context);

        void Draw();

    private:
        void DrawEntityNode(Engine::Entity entity);
        void DrawEmptySceneContextMenu();
        void OpenRenamePopup(Engine::Entity entity);
        void DrawRenamePopup();

    private:
        EditorContext &m_Context;
        Engine::EntityId m_RenameTarget{Engine::NullEntity};
        std::array<char, 128> m_RenameBuffer{};
    };
}