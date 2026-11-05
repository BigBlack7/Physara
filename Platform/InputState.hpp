#pragma once

/// @file
/// @brief 宿主私有的纯 CPU 输入累积器, 拥有状态, 事件 bank 和终止故障, 不执行 GLFW 操作.

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
    /// @brief 锁存第一个不可恢复输入错误及此后的拒绝计数, 不通过丢事件维持表面成功.
    struct InputFault
    {
        InputFaultReason reason;
        std::source_location source;
        std::uint64_t rejected = 0;
    };

    /// @brief 仅 owner 线程写入的双 bank 输入状态机, 固定存储位于宿主 heap State.
    /// @invariant 发布 bank 保持只读, 回调写入另一 bank; 对象不可移动以保护借用地址.
    class InputState final
    {
    public:
        InputState() = default;
        ~InputState() = default;
        InputState(const InputState &) = delete;
        InputState &operator=(const InputState &) = delete;
        InputState(InputState &&) = delete;
        InputState &operator=(InputState &&) = delete;

        /// @brief 仅一次建立初始窗口与光标状态, 非法值锁存终止故障.
        void Initialize(const WindowState &window, LogicalPoint position, CursorMode mode) noexcept;
        /// @brief 校验并按次序累积事实, 更新状态成功后才写入当前 bank.
        void Record(const InputEvent &event) noexcept;
        /// @brief 发布值快照并切换 bank, 清区间边沿和增量, 保留 held.
        /// @return 已锁存故障或无法继续分配序号时为空, 不交付成功的部分批次.
        [[nodiscard]] std::optional<HostFrameInput> Publish() noexcept;
        [[nodiscard]] const WindowState &Window() const noexcept;
        [[nodiscard]] const InputSnapshot &Snapshot() const noexcept;
        [[nodiscard]] const std::optional<InputFault> &Fault() const noexcept;
        [[nodiscard]] bool NeedsCursorRelease() const noexcept;
        /// @brief 废弃位移基线, 下一位置只建立基准, 避免焦点或模式切换造成跳变.
        void ResetMotion() noexcept;
        /// @brief 记录原生确认的模式, 切换时重置位移, 回到 Normal 后消除释放请求.
        void ObserveCursorMode(CursorMode mode) noexcept;
        /// @brief 清输入 held/边沿与累计运动, 请求宿主在安全边界释放原生光标捕获.
        void ResetForFailure() noexcept;
        /// @brief 锁存首错并累计拒绝次数, 同时执行失败状态清理.
        void Fail(InputFaultReason reason, std::source_location source = std::source_location::current()) noexcept;
    private:
        // 各重载只校验并更新对应 CPU 状态, 不发布事件或调用 GLFW; Record 统一维护到达顺序.
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
        // 尺寸/缩放变化共用同一修订号, 耗尽时锁存故障而不回绕.
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
