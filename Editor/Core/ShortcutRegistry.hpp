#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    enum class ShortcutContext
    {
        Global,
        Docked,
        SceneView,
        Presentation,
        CameraNavigation,
        Gizmo,
        SceneEditing
    };

    struct ShortcutAction
    {
        std::string commandId{};
        std::string name{};
        std::string group{};
        std::string keyChord{};
        ShortcutContext context{ShortcutContext::Global};
        std::string description{};
        ImGuiKey primaryKey{ImGuiKey_None};
        bool ctrl{false};
        bool shift{false};
        bool alt{false};
        bool implemented{false};
    };

    class ShortcutRegistry final
    {
    public:
        ShortcutRegistry();

        [[nodiscard]] const std::vector<ShortcutAction> &GetActions() const { return m_Actions; }
        [[nodiscard]] const ShortcutAction *FindAction(std::string_view commandId) const;
        [[nodiscard]] std::vector<const ShortcutAction *> GetActionsByGroup(std::string_view group) const;

        [[nodiscard]] bool IsPressed(std::string_view commandId) const;

    private:
        void RegisterDefaults();
        void Register(ShortcutAction action);

    private:
        std::vector<ShortcutAction> m_Actions{};
    };
}