#include "FrameStatistics.hpp"

/// @file
/// @brief 查询可用与包退休是独立事实. 读取到的秒数只交给原票据, 包 lease 继续由 RHI 管理.

#include <cassert>
#include <cmath>

namespace Physara::Runtime
{
    FrameStatistics::FrameStatistics(Platform::DeviceHost &host)
    {
        auto &device = *host.GetDevice();
        for (auto &interval : m_Intervals)
        {
            interval.query = device.CreateTimerQuery();
            if (!interval.query)
            {
                // 设备可能不支持 timestamps. 保留已取得的项并使用实际容量, 无有效句柄时仍可绘制.
                break;
            }
        }
    }

    FrameStatistics::TimerReservation FrameStatistics::ReserveTimer() const noexcept
    {
        using State = Editor::GpuIntervalState;
        auto unavailable = m_QueryFailed ? State::Failed : State::Unavailable;
        for (std::size_t i = 0; i < m_Intervals.size(); ++i)
        {
            const auto &interval = m_Intervals[i];
            if (interval.query)
            {
                if (!interval.awaiting)
                {
                    return {interval.query.Get(), i, State::Pending};
                }
                unavailable = State::Skipped;
            }
        }
        return {.state = unavailable};
    }

    void FrameStatistics::FinishTimer(TimerReservation timer, const Editor::SceneFrameIdentity *source) noexcept
    {
        auto &interval = m_Intervals[timer.index];
        assert(timer.query == interval.query.Get() && !interval.awaiting);
        interval.awaiting = true;
        interval.source = source ? std::optional(*source) : std::nullopt;
    }

    void FrameStatistics::FailTimer(TimerReservation timer) noexcept
    {
        auto &interval = m_Intervals[timer.index];
        assert(timer.query == interval.query.Get());
        interval = {};
        m_QueryFailed = true;
    }

    // 先观察可用性, 再读取已解析的结果. 无来源区间也要等两个时间戳可读才可复用句柄.
    void FrameStatistics::Poll(Platform::DeviceHost &host)
    {
        using State = Editor::GpuIntervalState;
        auto &device = *host.GetDevice();
        for (auto &interval : m_Intervals)
        {
            if (!interval.awaiting)
            {
                continue;
            }
            const auto scope = host.CaptureRhiErrors();
            const auto ready = device.PollTimerQuery(interval.query.Get());
            if (!ready && scope.IsClear())
            {
                continue;
            }
            // 零秒也可能是合法测量. 两次查询的诊断和 ready 共同决定本次读数能否发布.
            const auto seconds = ready ? static_cast<double>(device.GetTimerQueryTime(interval.query.Get())) : 0.0;
            const bool valid = ready && scope.IsClear() && std::isfinite(seconds) && seconds >= 0;
            if (interval.source)
            {
                const auto &source = *interval.source;
                if (m_Stats.latestScene && m_Stats.latestScene->source.submission == source.submission)
                {
                    m_Stats.latestScene->gpu = valid ? State::Ready : State::Failed;
                }
                if (valid && (!m_Stats.latestGpu || m_Stats.latestGpu->source.submission < source.submission))
                {
                    // 同次轮询可能收回多个池项. 显示顺序按原提交票据, 与遍历池项的顺序无关.
                    m_Stats.latestGpu = Editor::SceneGpuInterval{source, seconds};
                }
            }
            interval.awaiting = false;
            interval.source.reset();
            if (!valid)
            {
                interval.query.Reset();
                m_QueryFailed = true;
            }
        }
    }
}
