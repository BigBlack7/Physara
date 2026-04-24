#pragma once

#include <glm/vec2.hpp>

#include <Platform/Input/KeyCodes.hpp>

namespace Physara::Platform
{
    enum class CursorMode
    {
        Normal = 0,
        Hidden = 1,
        Locked = 2
    };

    class IInput
    {
    public:
        virtual ~IInput() = default;

        // 键盘查询
        virtual bool IsKeyDown(Key key) const = 0;
        virtual bool IsKeyPressed(Key key) const = 0;

        // 鼠标查询
        virtual bool IsMouseButtonDown(Mouse button) const = 0;
        virtual bool IsMouseButtonPressed(Mouse button) const = 0;
        virtual glm::vec2 GetMousePos() const = 0;
        virtual glm::vec2 GetMouseDelta() const = 0;
        virtual float GetScrollDelta() const = 0;

        // 光标控制
        virtual void SetCursorMode(CursorMode mode) = 0;

        // 帧管理(每帧开头调用)
        virtual void BeginFrame() = 0;
    };
}