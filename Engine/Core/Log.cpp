#include "Log.hpp"

#include <algorithm>
#include <array>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Physara::Engine
{

    namespace Internal
    {
        constexpr std::size_t kRingBufferCapacity = 512;
    }

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_EditorLogger = nullptr;
    std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> Log::s_RingBufferSink = nullptr;

    void Log::Init()
    {
        if (s_CoreLogger && s_EditorLogger && s_RingBufferSink)
        {
            return;
        }

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);
        consoleSink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%s:%#] %v%$");

        s_RingBufferSink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(Internal::kRingBufferCapacity);
        s_RingBufferSink->set_level(spdlog::level::trace);
        s_RingBufferSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%s:%#] %v");

        std::array<spdlog::sink_ptr, 2> sharedSinks{consoleSink, s_RingBufferSink};

        s_CoreLogger = std::make_shared<spdlog::logger>(
            "Engine",
            sharedSinks.begin(),
            sharedSinks.end());

        s_EditorLogger = std::make_shared<spdlog::logger>(
            "Editor",
            sharedSinks.begin(),
            sharedSinks.end());

        s_CoreLogger->set_level(spdlog::level::trace);
        s_EditorLogger->set_level(spdlog::level::trace);

        s_CoreLogger->flush_on(spdlog::level::err);
        s_EditorLogger->flush_on(spdlog::level::err);
    }

    void Log::Shutdown()
    {
        if (s_CoreLogger)
        {
            s_CoreLogger->flush();
        }
        if (s_EditorLogger)
        {
            s_EditorLogger->flush();
        }

        s_CoreLogger.reset();
        s_EditorLogger.reset();
        s_RingBufferSink.reset();
    }

    std::shared_ptr<spdlog::logger> &Log::GetCoreLogger()
    {
        return s_CoreLogger;
    }

    std::shared_ptr<spdlog::logger> &Log::GetEditorLogger()
    {
        return s_EditorLogger;
    }

    std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> &Log::GetRingBufferSink()
    {
        return s_RingBufferSink;
    }

    std::vector<std::string> Log::GetRecentLogs(std::size_t count)
    {
        if (!s_RingBufferSink)
        {
            return {};
        }

        const std::size_t clamped = std::min(count, Internal::kRingBufferCapacity);
        return s_RingBufferSink->last_formatted(clamped);
    }
}