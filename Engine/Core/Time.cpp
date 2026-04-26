#include "Time.hpp"

#include <algorithm>
#include <chrono>

namespace Physara::Engine
{
    namespace Internal
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
        const Internal::Clock::time_point now = Internal::Clock::now();

        if (!Internal::g_HasTicked)
        {
            Internal::g_HasTicked = true;
            Internal::g_lastFrameTime = now;
            Internal::g_DeltaTime = 0.;
            Internal::g_TotalTime = Internal::Seconds(now - Internal::g_StartTime).count();
            return;
        }

        const double rawDelta = Internal::Seconds(now - Internal::g_lastFrameTime).count();
        const double nonNegativeDelta = std::max(0., rawDelta);

        Internal::g_DeltaTime = std::min(nonNegativeDelta, Internal::kMaxDeltaTime);
        Internal::g_TotalTime = Internal::Seconds(now - Internal::g_StartTime).count();

        Internal::g_lastFrameTime = now;
    }

    double Time::GetDeltaTime()
    {
        return Internal::g_DeltaTime;
    }

    double Time::GetTotalTime()
    {
        return Internal::g_TotalTime;
    }
}