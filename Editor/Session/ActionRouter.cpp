#include "ActionRouter.hpp"

#include <variant>

/// @file
/// @brief 基于平台值合同集中解释失效边沿和业务优先级, 供会话和工作区共同使用.

namespace Physara::Editor::Detail
{
    InputOwner ResolveInputOwner(const InputClaims &claims, const Platform::InputEvent &event) noexcept
    {
        // 先区分键盘和指针通道, 避免把字符或窗口事实当成业务动作触发.
        const bool keyboard = std::holds_alternative<Platform::KeyEvent>(event);
        const bool pointer = std::holds_alternative<Platform::MouseButtonEvent>(event) ||
                             std::holds_alternative<Platform::ScrollEvent>(event) ||
                             std::holds_alternative<Platform::CursorPositionEvent>(event);
        if (!keyboard && !pointer)
        {
            return InputOwner::None;
        }
        // 优先交给本通道的 UI 占用者, 然后依次检查活动 Gizmo 和宿主已生效的捕获.
        const bool ui = keyboard ? claims.uiKeyboard : claims.uiPointer;
        if (claims.blockingPopup || ui)
        {
            return InputOwner::Ui;
        }
        if (claims.gizmoActive)
        {
            return InputOwner::Gizmo;
        }
        if (claims.cursorMode == Platform::CursorMode::Relative)
        {
            return InputOwner::Camera;
        }
        // 未占用的按键由工作区匹配动作, 未占用的指针仅在视口内容命中时有归属.
        if (keyboard)
        {
            return InputOwner::Workspace;
        }
        return claims.viewportHit ? InputOwner::Viewport : InputOwner::None;
    }

    bool InterruptsInput(const Platform::InputEvent &event) noexcept
    {
        if (const auto *focus = std::get_if<Platform::FocusEvent>(&event))
        {
            return !focus->focused;
        }
        if (const auto *iconify = std::get_if<Platform::IconifyEvent>(&event))
        {
            return iconify->iconified;
        }
        if (const auto *size = std::get_if<Platform::FramebufferSizeEvent>(&event))
        {
            return size->size.width == 0 || size->size.height == 0;
        }
        return false;
    }
}
