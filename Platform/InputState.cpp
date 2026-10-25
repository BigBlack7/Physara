#include "InputState.hpp"

#include <cmath>
#include <limits>

namespace
{
    bool IsFinite(Physara::Platform::LogicalPoint value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }
    bool ValidScale(Physara::Platform::ContentScale value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && value.x > 0.0f && value.y > 0.0f;
    }
    void UpdateButton(Physara::Platform::ButtonState &button, Physara::Platform::KeyAction action) noexcept
    {
        using Physara::Platform::KeyAction;
        if (action == KeyAction::Press)
        {
            button.down = true;
            button.pressed = true;
        }
        else if (action == KeyAction::Release)
        {
            button.down = false;
            button.released = true;
        }
    }
}

namespace Physara::Platform::Detail
{
    void InputState::Initialize(const WindowState &window, LogicalPoint position, CursorMode mode) noexcept
    {
        if (m_Initialized || !ValidScale(window.contentScale) || !IsFinite(position) || mode > CursorMode::Relative)
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        m_Window = window;
        m_Window.metricsRevision = 1;
        m_Snapshot.cursorPositionLogical = position;
        m_Snapshot.cursorMode = mode;
        m_Initialized = true;
    }

    void InputState::Fail(InputFaultReason reason, std::source_location source) noexcept
    {
        if (!m_Fault)
        {
            m_Fault = InputFault{reason, source, 1};
        }
        else if (m_Fault->rejected != std::numeric_limits<std::uint64_t>::max())
        {
            ++m_Fault->rejected;
        }
        ResetForFailure();
    }

    void InputState::Record(const InputEvent &event) noexcept
    {
        if (m_Fault)
        {
            Fail(m_Fault->reason);
            return;
        }
        if (!m_Initialized)
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        if (m_PendingCount == InputEventCapacity)
        {
            Fail(InputFaultReason::Overflow);
            return;
        }
        std::visit([this](const auto &value) noexcept { Apply(value); }, event);
        if (!m_Fault)
        {
            m_Banks[m_PendingBank][m_PendingCount++] = event;
        }
    }

    std::optional<HostFrameInput> InputState::Publish() noexcept
    {
        if (m_Fault)
        {
            return std::nullopt;
        }
        if (!m_Initialized)
        {
            Fail(InputFaultReason::InvalidData);
            return std::nullopt;
        }
        if (m_Sequence == std::numeric_limits<std::uint64_t>::max())
        {
            Fail(InputFaultReason::CounterExhausted);
            return std::nullopt;
        }
        HostFrameInput frame{++m_Sequence, m_Window, m_Snapshot, {m_Banks[m_PendingBank].data(), m_PendingCount}};
        m_PendingBank = 1 - m_PendingBank;
        m_PendingCount = 0;
        for (auto &key : m_Snapshot.keys)
        {
            key.pressed = false;
            key.released = false;
        }
        for (auto &button : m_Snapshot.mouseButtons)
        {
            button.pressed = false;
            button.released = false;
        }
        m_Snapshot.cursorDeltaLogical = {};
        m_Snapshot.scrollOffset = {};
        return frame;
    }

    const WindowState &InputState::Window() const noexcept
    {
        return m_Window;
    }
    const InputSnapshot &InputState::Snapshot() const noexcept
    {
        return m_Snapshot;
    }
    const std::optional<InputFault> &InputState::Fault() const noexcept
    {
        return m_Fault;
    }
    bool InputState::NeedsCursorRelease() const noexcept
    {
        return m_CursorReleasePending;
    }
    void InputState::ResetMotion() noexcept
    {
        m_MotionBaseline = false;
        m_Snapshot.cursorDeltaLogical = {};
    }
    void InputState::ObserveCursorMode(CursorMode mode) noexcept
    {
        if (mode != m_Snapshot.cursorMode)
        {
            ResetMotion();
        }
        m_Snapshot.cursorMode = mode;
        if (mode == CursorMode::Normal)
        {
            m_CursorReleasePending = false;
        }
    }
    void InputState::ResetForFailure() noexcept
    {
        for (auto &key : m_Snapshot.keys)
        {
            key = {};
        }
        for (auto &button : m_Snapshot.mouseButtons)
        {
            button = {};
        }
        m_Snapshot.scrollOffset = {};
        ResetMotion();
        m_CursorReleasePending = true;
    }

    void InputState::Apply(KeyEvent event) noexcept
    {
        if (event.action > KeyAction::Repeat || static_cast<unsigned>(event.modifiers) > 63 ||
            (event.key != Key::Unknown && !IsKnown(event.key)))
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        if (IsKnown(event.key))
        {
            UpdateButton(m_Snapshot.keys[static_cast<std::size_t>(event.key) - 1], event.action);
        }
    }
    void InputState::Apply(CharEvent event) noexcept
    {
        if (event.codepoint > 0x10FFFF || (event.codepoint >= 0xD800 && event.codepoint <= 0xDFFF))
        {
            Fail(InputFaultReason::InvalidData);
        }
    }
    void InputState::Apply(MouseButtonEvent event) noexcept
    {
        if (!IsKnown(event.button) || event.action > KeyAction::Release || static_cast<unsigned>(event.modifiers) > 63)
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        UpdateButton(m_Snapshot.mouseButtons[static_cast<std::size_t>(event.button)], event.action);
    }
    void InputState::Apply(CursorPositionEvent event) noexcept
    {
        if (!IsFinite(event.position))
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        if (m_Window.focused && m_MotionBaseline)
        {
            LogicalPoint delta{
                m_Snapshot.cursorDeltaLogical.x + (event.position.x - m_Snapshot.cursorPositionLogical.x),
                m_Snapshot.cursorDeltaLogical.y + (event.position.y - m_Snapshot.cursorPositionLogical.y)};
            if (!IsFinite(delta))
            {
                Fail(InputFaultReason::InvalidData);
                return;
            }
            m_Snapshot.cursorDeltaLogical = delta;
        }
        m_Snapshot.cursorPositionLogical = event.position;
        m_MotionBaseline = m_Window.focused;
    }
    void InputState::Apply(CursorEnterEvent event) noexcept
    {
        m_Snapshot.cursorInside = event.entered;
    }
    void InputState::Apply(ScrollEvent event) noexcept
    {
        LogicalPoint sum{m_Snapshot.scrollOffset.x + event.offset.x, m_Snapshot.scrollOffset.y + event.offset.y};
        if (!IsFinite(event.offset) || !IsFinite(sum))
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        m_Snapshot.scrollOffset = sum;
    }
    bool InputState::AdvanceMetrics() noexcept
    {
        if (m_Window.metricsRevision == std::numeric_limits<std::uint64_t>::max())
        {
            Fail(InputFaultReason::CounterExhausted);
            return false;
        }
        ++m_Window.metricsRevision;
        return true;
    }
    void InputState::Apply(ClientSizeEvent event) noexcept
    {
        if (event.size != m_Window.clientSizeLogical && AdvanceMetrics())
        {
            m_Window.clientSizeLogical = event.size;
        }
    }
    void InputState::Apply(FramebufferSizeEvent event) noexcept
    {
        if (event.size != m_Window.framebufferSizePixels && AdvanceMetrics())
        {
            m_Window.framebufferSizePixels = event.size;
        }
    }
    void InputState::Apply(ContentScaleEvent event) noexcept
    {
        if (!ValidScale(event.scale))
        {
            Fail(InputFaultReason::InvalidData);
            return;
        }
        if (event.scale != m_Window.contentScale && AdvanceMetrics())
        {
            m_Window.contentScale = event.scale;
        }
    }
    void InputState::Apply(FocusEvent event) noexcept
    {
        m_Window.focused = event.focused;
        ResetMotion();
        if (!event.focused)
        {
            // GLFW 随后送出 release 回调; 此处只清 held/置 edge, 不合成第二份物理事件.
            for (auto &key : m_Snapshot.keys)
            {
                key.released = key.released || key.down;
                key.down = false;
            }
            for (auto &button : m_Snapshot.mouseButtons)
            {
                button.released = button.released || button.down;
                button.down = false;
            }
            m_CursorReleasePending = true;
        }
    }
    void InputState::Apply(IconifyEvent event) noexcept
    {
        m_Window.iconified = event.iconified;
    }
    void InputState::Apply(CloseRequestedEvent event) noexcept
    {
        m_Window.closeRequested = event.requested;
    }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
    void InputState::SetRejectedForChecks(std::uint64_t rejected) noexcept
    {
        if (m_Fault)
        {
            m_Fault->rejected = rejected;
        }
    }
    void InputState::SetCountersForChecks(std::uint64_t sequence, std::uint64_t revision) noexcept
    {
        m_Sequence = sequence;
        m_Window.metricsRevision = revision;
    }
#endif
}
