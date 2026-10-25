#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>

namespace Physara::Platform
{
    inline constexpr std::size_t InputEventCapacity = 4096;

    // 逻辑尺寸与像素尺寸按字段名区分; 不把 contentScale 当两者的比例.
    struct WindowExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool operator==(const WindowExtent &) const = default;
    };
    struct LogicalPoint
    {
        double x = 0.0;
        double y = 0.0;
        bool operator==(const LogicalPoint &) const = default;
    };
    struct ContentScale
    {
        float x = 1.0f;
        float y = 1.0f;
        bool operator==(const ContentScale &) const = default;
    };
    struct WindowState
    {
        WindowExtent clientSizeLogical;
        WindowExtent framebufferSizePixels;
        ContentScale contentScale;
        bool focused = false;
        bool iconified = false;
        bool closeRequested = false;
        std::uint64_t metricsRevision = 0;
        [[nodiscard]] bool Renderable() const noexcept
        {
            return !iconified && framebufferSizePixels.width > 0 && framebufferSizePixels.height > 0;
        }
    };

    // 键集合对应固定 GLFW 公共协议, 数值为项目自己的紧凑索引, 不暴露原生码.
    enum class Key : std::uint16_t
    {
        Unknown,
        Space,
        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        Semicolon,
        Equal,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LeftBracket,
        Backslash,
        RightBracket,
        GraveAccent,
        World1,
        World2,
        Escape,
        Enter,
        Tab,
        Backspace,
        Insert,
        Delete,
        Right,
        Left,
        Down,
        Up,
        PageUp,
        PageDown,
        Home,
        End,
        CapsLock,
        ScrollLock,
        NumLock,
        PrintScreen,
        Pause,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        Keypad0,
        Keypad1,
        Keypad2,
        Keypad3,
        Keypad4,
        Keypad5,
        Keypad6,
        Keypad7,
        Keypad8,
        Keypad9,
        KeypadDecimal,
        KeypadDivide,
        KeypadMultiply,
        KeypadSubtract,
        KeypadAdd,
        KeypadEnter,
        KeypadEqual,
        LeftShift,
        LeftControl,
        LeftAlt,
        LeftSuper,
        RightShift,
        RightControl,
        RightAlt,
        RightSuper,
        Menu,
        Count
    };
    inline constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count) - 1;
    enum class MouseButton : std::uint8_t
    {
        Primary,
        Secondary,
        Middle,
        Button4,
        Button5,
        Button6,
        Button7,
        Button8,
        Count
    };
    inline constexpr std::size_t MouseButtonCount = static_cast<std::size_t>(MouseButton::Count);
    enum class KeyAction : std::uint8_t
    {
        Press,
        Release,
        Repeat
    };
    enum class Modifier : std::uint8_t
    {
        None = 0,
        Shift = 1,
        Control = 2,
        Alt = 4,
        Super = 8,
        CapsLock = 16,
        NumLock = 32
    };
    enum class CursorMode : std::uint8_t
    {
        Normal,
        Relative
    };
    [[nodiscard]] constexpr bool IsKnown(Key key) noexcept
    {
        return key > Key::Unknown && key < Key::Count;
    }
    [[nodiscard]] constexpr bool IsKnown(MouseButton button) noexcept
    {
        return button < MouseButton::Count;
    }
    struct ButtonState
    {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };
    struct InputSnapshot
    {
        std::array<ButtonState, KeyCount> keys{};
        std::array<ButtonState, MouseButtonCount> mouseButtons{};
        LogicalPoint cursorPositionLogical;
        LogicalPoint cursorDeltaLogical;
        // 原生滚轮 offset, 不是像素, 不乘 content scale.
        LogicalPoint scrollOffset;
        bool cursorInside = false;
        CursorMode cursorMode = CursorMode::Normal;
        [[nodiscard]] ButtonState KeyState(Key key) const noexcept
        {
            return IsKnown(key) ? keys[static_cast<std::size_t>(key) - 1] : ButtonState{};
        }
        [[nodiscard]] ButtonState MouseState(MouseButton button) const noexcept
        {
            return IsKnown(button) ? mouseButtons[static_cast<std::size_t>(button)] : ButtonState{};
        }
    };

    struct KeyEvent
    {
        Key key = Key::Unknown;
        std::int32_t scanCode = 0;
        KeyAction action = KeyAction::Release;
        Modifier modifiers = Modifier::None;
    };

    struct CharEvent
    {
        char32_t codepoint = 0;
    };

    struct MouseButtonEvent
    {
        MouseButton button = MouseButton::Primary;
        KeyAction action = KeyAction::Release;
        Modifier modifiers = Modifier::None;
    };

    struct CursorPositionEvent
    {
        LogicalPoint position;
    };

    struct CursorEnterEvent
    {
        bool entered = false;
    };

    struct ScrollEvent
    {
        LogicalPoint offset;
    };

    struct ClientSizeEvent
    {
        WindowExtent size;
    };

    struct FramebufferSizeEvent
    {
        WindowExtent size;
    };

    struct ContentScaleEvent
    {
        ContentScale scale;
    };

    struct FocusEvent
    {
        bool focused = false;
    };

    struct IconifyEvent
    {
        bool iconified = false;
    };

    struct CloseRequestedEvent
    {
        bool requested = false;
    };

    using InputEvent = std::variant<KeyEvent, CharEvent, MouseButtonEvent, CursorPositionEvent, CursorEnterEvent,
                                    ScrollEvent, ClientSizeEvent, FramebufferSizeEvent, ContentScaleEvent, FocusEvent,
                                    IconifyEvent, CloseRequestedEvent>;
    static_assert(sizeof(InputEvent) <= 64);
    static_assert(std::is_nothrow_copy_constructible_v<InputEvent> && std::is_nothrow_copy_assignable_v<InputEvent>);
    struct SetCursorMode
    {
        CursorMode mode = CursorMode::Normal;
    };
    struct SetCloseRequested
    {
        bool requested = false;
    };
    using HostRequest = std::variant<SetCursorMode, SetCloseRequested>;

    // 由 owner/main 线程发布. 值成员自持; events 只读借用到下一次接受的 owner poll 开始或关闭.
    // Wait/Apply 不改写已发布 bank; 跨帧或线程保存 events 必须由消费者复制.
    struct HostFrameInput
    {
        std::uint64_t sequence = 0;
        WindowState window;
        InputSnapshot state;
        std::span<const InputEvent> events;
    };
}
