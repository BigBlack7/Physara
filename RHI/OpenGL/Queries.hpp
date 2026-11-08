#pragma once

/// @file
/// @brief 单 Graphics 队列的提交观察者和 GPU 时间戳对, 共用 Device 的完成与删除协议.
/// 状态操作限 owner/current context; 已发布句柄的 worker 最终 Release 只交接 CPU 所有权.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "Backend.hpp"

namespace Physara::RHI::OpenGL
{
    inline constexpr std::size_t TimerNameLimit = 512; ///< 每 Device 的存活及待删除名字总数, 每 Timer 占两个.

    /// @brief 强持 Device 的已接受提交快照, 不插入原生 fence, 不保活第二份工作包.
    /// @note 零 ID 只表示无捕获提交; Reset 不取消工作, 未提交即时前缀仍由原协议保护.
    class OpenGLEventQuery final : public RefCounter<RHI::EventQuery>
    {
    public:
        explicit OpenGLEventQuery(OpenGLDevice *device) noexcept : m_Device(device)
        {
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        std::uint64_t m_SubmissionId = 0;
        friend class OpenGLDevice;
    };

    /// @brief 可独立重叠的 GPU 时间戳对, available 仅证明计时结果, 不授权回收整个提交包.
    /// @note 包 lease 保活整个 wrapper; 它强持 Device, 析构转交两个预分配删除节点.
    /// m_Packet 只借用最近一次使用的稳定包, owner 在该包退休前解除, 不形成反向强引用环.
    class OpenGLTimerQuery final : public RefCounter<RHI::TimerQuery>
    {
    public:
        /// @brief 准备 CPU wrapper/删除节点; 工厂随后取得整对名字, 成功前不发布对象.
        OpenGLTimerQuery(OpenGLDevice *device, GLint counterBits);
        ~OpenGLTimerQuery() override;
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }

    private:
        /// @brief Idle -> Begun -> Ended -> Ready; 录制失效或原生取值错误进入 Failed.
        /// @note Ready 可 Reset 或由另一 recording 的 Begin 消费, 仍不解除旧包 lease.
        /// Failed 且仍关联包时不能 Reset, 须先由原包完成退休或受控退出.
        enum class Phase : std::uint8_t
        {
            Idle,
            Begun,
            Ended,
            Ready,
            Failed
        };
        /// @brief owner 接收录制失效/worker abandon, 已发点继续保留到原包退休.
        void ObserveRecording() noexcept;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        std::array<std::unique_ptr<NativeDeletion>, 2> m_Deletion;
        const GLint m_CounterBits; ///< 纳秒 counter 的实际有效位, 合法区间小于一个周期.
        UploadWorkPacket *m_Packet = nullptr; ///< 最近使用的借用; Ready/Reset 均不证明原包已退休.
        std::uint64_t m_LastRecordingId = 0; ///< Reset 也不抹去, 禁止同 recording 再次 Begin.
        Phase m_Phase = Phase::Idle;
        float m_Seconds = 0; ///< 仅 Ready 时为有效缓存; 零秒也可能是合法结果.
        bool m_Published = false; ///< 发布后析构仅入删除 FIFO, 不在 final Release 直接执行 GL.
        friend class OpenGLDevice;
        friend class OpenGLCommandList;
    };
} // namespace Physara::RHI::OpenGL
