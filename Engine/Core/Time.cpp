#include "Time.hpp"

#include <algorithm>
#include <chrono>

namespace Physara::Engine
{
    namespace TimeDetail
    {
        using Clock = std::chrono::steady_clock;
        using Seconds = std::chrono::duration<double>;

        constexpr double kMaxDeltaTime = 0.1;

        const Clock::time_point g_StartTime = Clock::now();
        Clock::time_point g_lastFrameTime = g_StartTime;

        double g_DeltaTime = 0.;
        double g_TotalTime = 0.;
        bool g_HasTicked = false;
    }

    void Time::Tick()
    {
        const TimeDetail::Clock::time_point now = TimeDetail::Clock::now();

        if (!TimeDetail::g_HasTicked)
        {
            TimeDetail::g_HasTicked = true;
            TimeDetail::g_lastFrameTime = now;
            TimeDetail::g_DeltaTime = 0.;
            TimeDetail::g_TotalTime = TimeDetail::Seconds(now - TimeDetail::g_StartTime).count();
            return;
        }

        const double rawDelta = TimeDetail::Seconds(now - TimeDetail::g_lastFrameTime).count();
        const double nonNegativeDelta = std::max(0., rawDelta);

        TimeDetail::g_DeltaTime = std::min(nonNegativeDelta, TimeDetail::kMaxDeltaTime);
        TimeDetail::g_TotalTime = TimeDetail::Seconds(now - TimeDetail::g_StartTime).count();

        TimeDetail::g_lastFrameTime = now;
    }

    double Time::GetDeltaTime()
    {
        return TimeDetail::g_DeltaTime;
    }

    double Time::GetTotalTime()
    {
        return TimeDetail::g_TotalTime;
    }
}