#pragma once

// 宿主私有的纯 CPU 累积器: 持有窗口/按键状态、两个事件 bank 和终止故障, 不执行 GLFW 操作.

#include "Input.hpp"

#include <optional>
#include <source_location>

#include <Core/Error.hpp>

namespace Physara::Platform::Detail
{
    enum class InputFaultReason : std::uint8_t
    {
        Overflow,
        InvalidData,
        CounterExhausted
    };
    struct InputFault
    {
        InputFaultReason reason;
        std::source_location source;
        std::uint64_t rejected = 0;
    };

    // 仅 owner 线程写入. 两个固定 bank 原位构造于宿主 heap State, 不可移动以保护借用.
    class InputState final
    {
    public:
        InputState() = default;
        ~InputState() = default;
        InputState(const InputState &) = delete;
        InputState &operator=(const InputState &) = delete;
        InputState(InputState &&) = delete;
        InputState &operator=(InputState &&) = delete;

        // Initialize 仅一次; Record 保留次序并校验值. 首个终止故障锁存后, Publish 不再交付成功批次.
        // Publish 切换 bank 并重置区间边沿, 不重置 held; 原生 capture 的实际释放由宿主安全边界执行.
        void Initialize(const WindowState &window, LogicalPoint position, CursorMode mode) noexcept;
        void Record(const InputEvent &event) noexcept;
        [[nodiscard]] std::optional<HostFrameInput> Publish() noexcept;
        [[nodiscard]] const WindowState &Window() const noexcept;
        [[nodiscard]] const InputSnapshot &Snapshot() const noexcept;
        [[nodiscard]] const std::optional<InputFault> &Fault() const noexcept;
        [[nodiscard]] bool NeedsCursorRelease() const noexcept;
        void ResetMotion() noexcept;
        void ObserveCursorMode(CursorMode mode) noexcept;
        void ResetForFailure() noexcept;
        void Fail(InputFaultReason reason, std::source_location source = std::source_location::current()) noexcept;
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        void SetCountersForChecks(std::uint64_t sequence, std::uint64_t revision) noexcept;
        void SetRejectedForChecks(std::uint64_t rejected) noexcept;
#endif
    private:
        void Apply(KeyEvent event) noexcept;
        void Apply(CharEvent event) noexcept;
        void Apply(MouseButtonEvent event) noexcept;
        void Apply(CursorPositionEvent event) noexcept;
        void Apply(CursorEnterEvent event) noexcept;
        void Apply(ScrollEvent event) noexcept;
        void Apply(ClientSizeEvent event) noexcept;
        void Apply(FramebufferSizeEvent event) noexcept;
        void Apply(ContentScaleEvent event) noexcept;
        void Apply(FocusEvent event) noexcept;
        void Apply(IconifyEvent event) noexcept;
        void Apply(CloseRequestedEvent event) noexcept;
        [[nodiscard]] bool AdvanceMetrics() noexcept;

        std::array<std::array<InputEvent, InputEventCapacity>, 2> m_Banks{};
        WindowState m_Window;
        InputSnapshot m_Snapshot;
        std::optional<InputFault> m_Fault;
        std::uint64_t m_Sequence = 0;
        std::size_t m_PendingBank = 0;
        std::size_t m_PendingCount = 0;
        bool m_Initialized = false;
        bool m_MotionBaseline = false;
        bool m_CursorReleasePending = false;
    };
}
