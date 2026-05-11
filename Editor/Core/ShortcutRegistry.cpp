#include "ShortcutRegistry.hpp"

#include <utility>

namespace Physara::Editor
{
    namespace Internal
    {
        bool ModifiersMatch(const ShortcutAction &action)
        {
            const ImGuiIO &io = ImGui::GetIO();
            return io.KeyCtrl == action.ctrl &&
                   io.KeyShift == action.shift &&
                   io.KeyAlt == action.alt;
        }
    }

    ShortcutRegistry::ShortcutRegistry()
    {
        RegisterDefaults();
    }

    const ShortcutAction *ShortcutRegistry::FindAction(std::string_view commandId) const
    {
        for (const ShortcutAction &action : m_Actions)
        {
            if (action.commandId == commandId)
            {
                return &action;
            }
        }

        return nullptr;
    }

    std::vector<const ShortcutAction *> ShortcutRegistry::GetActionsByGroup(std::string_view group) const
    {
        std::vector<const ShortcutAction *> result;
        for (const ShortcutAction &action : m_Actions)
        {
            if (action.group == group)
            {
                result.push_back(&action);
            }
        }

        return result;
    }

    bool ShortcutRegistry::IsPressed(std::string_view commandId) const
    {
        const ShortcutAction *action = FindAction(commandId);
        if (action == nullptr || action->primaryKey == ImGuiKey_None)
        {
            return false;
        }

        return Internal::ModifiersMatch(*action) && ImGui::IsKeyPressed(action->primaryKey, false);
    }

    void ShortcutRegistry::RegisterDefaults()
    {
        Register({"help.shortcuts", "Help / Shortcuts", "Global", "F1", ShortcutContext::Global,
                  "Show every available editor shortcut.", ImGuiKey_F1, false, false, false, true});
        Register({"viewport.clean.toggle", "Clean Viewport", "Global", "F10", ShortcutContext::Global,
                  "Toggle clean Scene View mode without viewport overlays.", ImGuiKey_F10, false, false, false, true});
        Register({"viewport.presentation.toggle", "Presentation Mode", "Global", "F11", ShortcutContext::Global,
                  "Toggle pure viewport presentation mode.", ImGuiKey_F11, false, false, false, true});
        Register({"capture.current_view", "Capture Current View", "Global", "F12", ShortcutContext::Global,
                  "Request a capture of the current rendered view.", ImGuiKey_F12, false, false, false, true});
        Register({"viewport.presentation.exit", "Exit Presentation", "Presentation", "Esc", ShortcutContext::Presentation,
                  "Return from presentation mode to the editor workspace.", ImGuiKey_Escape, false, false, false, true});

        Register({"gizmo.translate", "Translate Tool", "Gizmo", "W", ShortcutContext::Gizmo,
                  "Select the translate gizmo when camera navigation is inactive.", ImGuiKey_W});
        Register({"gizmo.rotate", "Rotate Tool", "Gizmo", "E", ShortcutContext::Gizmo,
                  "Select the rotate gizmo when camera navigation is inactive.", ImGuiKey_E});
        Register({"gizmo.scale", "Scale Tool", "Gizmo", "R", ShortcutContext::Gizmo,
                  "Select the scale gizmo when camera navigation is inactive.", ImGuiKey_R});
        Register({"gizmo.space.toggle", "Toggle Local / World", "Gizmo", "X", ShortcutContext::Gizmo,
                  "Toggle gizmo coordinate space.", ImGuiKey_X});

        Register({"camera.forward", "Move Forward", "Camera Navigation", "W", ShortcutContext::CameraNavigation,
                  "Move the editor camera forward while roaming.", ImGuiKey_W});
        Register({"camera.left", "Move Left", "Camera Navigation", "A", ShortcutContext::CameraNavigation,
                  "Move the editor camera left while roaming.", ImGuiKey_A});
        Register({"camera.backward", "Move Backward", "Camera Navigation", "S", ShortcutContext::CameraNavigation,
                  "Move the editor camera backward while roaming.", ImGuiKey_S});
        Register({"camera.right", "Move Right", "Camera Navigation", "D", ShortcutContext::CameraNavigation,
                  "Move the editor camera right while roaming.", ImGuiKey_D});
        Register({"camera.down", "Move Down", "Camera Navigation", "Q", ShortcutContext::CameraNavigation,
                  "Move the editor camera down while roaming.", ImGuiKey_Q});
        Register({"camera.up", "Move Up", "Camera Navigation", "E", ShortcutContext::CameraNavigation,
                  "Move the editor camera up while roaming.", ImGuiKey_E});
        Register({"camera.roam", "Roam Mode", "Camera Navigation", "` / Right Mouse", ShortcutContext::CameraNavigation,
                  "Enter editor camera roam mode.", ImGuiKey_GraveAccent});

        Register({"scene.delete", "Delete Entity", "Scene Editing", "Backspace", ShortcutContext::SceneEditing,
                  "Delete the selected entity.", ImGuiKey_Backspace, false, false, false, true});
        Register({"scene.save", "Save Scene", "Scene Editing", "Ctrl+S", ShortcutContext::SceneEditing,
                  "Save the active scene.", ImGuiKey_S, true, false, false, true});
    }

    void ShortcutRegistry::Register(ShortcutAction action)
    {
        m_Actions.push_back(std::move(action));
    }
}