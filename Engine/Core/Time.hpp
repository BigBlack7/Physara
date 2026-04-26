#pragma once

namespace Physara::Engine
{
    class Time final
    {
    public:
        Time() = delete;
        Time(const Time &) = delete;
        Time &operator=(const Time &) = delete;

        static void Tick();

        static double GetDeltaTime();
        static double GetTotalTime();
    };
}