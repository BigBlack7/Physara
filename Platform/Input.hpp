#pragma once

/// @file
/// @brief 窗口事实, 输入事件和平台请求的值合同, 原生枚举不穿过此边界.
/// 逻辑坐标, framebuffer 像素和滚轮 offset 使用各自单位, 消费者按目标空间转换.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>

namespace Physara::Platform
{
    inline constexpr std::size_t InputEventCapacity = 4096;

    /// @brief 无符号宽高, 所在字段名决定逻辑单位或像素; 零尺寸是可观察的暂停状态.
    struct WindowExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool operator==(const WindowExtent &) const = default;
    };
    /// @brief 二维输入值, 光标使用左上原点, x 向右/y 向下的逻辑单位, 滚轮字段另用原生 offset 单位.
    struct LogicalPoint
    {
        double x = 0.0;
        double y = 0.0;
        bool operator==(const LogicalPoint &) const = default;
    };
    /// @brief 原生内容缩放观察值, 不保证等于 framebuffer 尺寸与逻辑尺寸的比值.
    struct ContentScale
    {
        float x = 1.0f;
        float y = 1.0f;
        bool operator==(const ContentScale &) const = default;
    };
    /// @brief 发布边界的窗口状态, metricsRevision 标识尺寸或内容缩放的变化.
    struct WindowState
    {
        WindowExtent clientSizeLogical;
        WindowExtent framebufferSizePixels;
        ContentScale contentScale;
        bool focused = false;
        bool iconified = false;
        bool closeRequested = false;
        std::uint64_t metricsRevision = 0;
        /// @brief 窗口未最小化且 framebuffer 两维非零时可渲染, 与是否有焦点独立.
        [[nodiscard]] bool Renderable() const noexcept
        {
            return !iconified && framebufferSizePixels.width > 0 && framebufferSizePixels.height > 0;
        }
    };

    /// @brief 对应固定 GLFW 公共协议的键集合, 数值为项目自己的紧凑索引, 不暴露原生码.
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
    /// @brief 发布间隔末的 held 值及区间内边沿, 同一区间 pressed/released 可以同时为真.
    struct ButtonState
    {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };
    /// @brief 一次发布的按键末值与区间累计量, 有序事件用于恢复区间内的实际顺序.
    struct InputSnapshot
    {
        // 同一发布间隔的 press/release 可同时为真; down 表示间隔末值, events 保留实际到达顺序.
        // cursorDeltaLogical/scrollOffset 只累计当前间隔, 下一次成功发布后从零累计.
        std::array<ButtonState, KeyCount> keys{};
        std::array<ButtonState, MouseButtonCount> mouseButtons{};
        LogicalPoint cursorPositionLogical;
        LogicalPoint cursorDeltaLogical;
        // 原生滚轮 offset, 不是像素, 不乘 content scale.
        LogicalPoint scrollOffset;
        bool cursorInside = false;
        CursorMode cursorMode = CursorMode::Normal;
        /// @brief 按项目枚举查询按键/鼠标状态, 未知或越界值返回未按下的空状态.
        [[nodiscard]] ButtonState KeyState(Key key) const noexcept
        {
            return IsKnown(key) ? keys[static_cast<std::size_t>(key) - 1] : ButtonState{};
        }
        [[nodiscard]] ButtonState MouseState(MouseButton button) const noexcept
        {
            return IsKnown(button) ? mouseButtons[static_cast<std::size_t>(button)] : ButtonState{};
        }
    };

    /// @brief 物理按键事实; Unknown 仍保留 scanCode, 文本输入由独立 CharEvent 表达.
    struct KeyEvent
    {
        Key key = Key::Unknown;
        std::int32_t scanCode = 0;
        KeyAction action = KeyAction::Release;
        Modifier modifiers = Modifier::None;
    };

    /// @brief 单个 Unicode 标量值, 不以键盘布局或 Key 推导文本.
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

    /// @brief 原生滚轮 offset, 不是像素或逻辑距离, 不乘内容缩放.
    struct ScrollEvent
    {
        LogicalPoint offset;
    };

    /// @brief 客户区逻辑尺寸变化, 与下一个事件类型中的 framebuffer 像素尺寸分开处理.
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

    /// @brief 回调按到达次序发布的自持事实; 小值载荷允许固定容量 bank 无分配地保存.
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
    /// @brief 消费者交给宿主的意图, 与输入事实分开, 由宿主在安全边界顺序执行.
    using HostRequest = std::variant<SetCursorMode, SetCloseRequested>;

    /// @brief owner/main 线程发布的一批输入, sequence 按成功发布递增.
    /// @note 值成员自持; events 只读借用到下一次接受的 owner poll 开始或关闭.
    /// Wait/Apply 不改写已发布 bank; 跨帧或线程保存 events 必须由消费者复制.
    struct HostFrameInput
    {
        std::uint64_t sequence = 0;
        WindowState window;
        InputSnapshot state;
        std::span<const InputEvent> events;
    };
}
