#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/ringbuffer_sink.h>

namespace Physara::Engine
{
    class Log
    {
    public:
        static void Init();
        static void Shutdown();

        static std::shared_ptr<spdlog::logger> &GetCoreLogger();
        static std::shared_ptr<spdlog::logger> &GetEditorLogger();

        // 供LogPanel拉取环形日志
        static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> &GetRingBufferSink();

        // 拉取最近指定条格式化日志
        static std::vector<std::string> GetRecentLogs(std::size_t count = 512);

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_EditorLogger;
        static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> s_RingBufferSink;
    };
}

// ---------- 内部宏工具 ----------

#define PHYSARA_LOG_IMPL(loggerGetterExpr, levelEnum, ...)                                 \
    do                                                                                     \
    {                                                                                      \
        auto &_physara_logger_ref = (loggerGetterExpr);                                    \
        if (_physara_logger_ref)                                                           \
        {                                                                                  \
            _physara_logger_ref->log(                                                      \
                spdlog::source_loc{__FILE__, static_cast<int>(__LINE__), SPDLOG_FUNCTION}, \
                (levelEnum),                                                               \
                __VA_ARGS__);                                                              \
        }                                                                                  \
    } while (false) // 作为一个单独语句不破坏if/for等结构

#if defined(PHYSARA_DEBUG)
#include <cassert>
#define PHYSARA_FATAL_TRAP()              \
    do                                    \
    {                                     \
        assert(false && "PHYSARA_FATAL"); \
    } while (false)
#else
#include <cstdlib>
#define PHYSARA_FATAL_TRAP() \
    do                       \
    {                        \
        std::abort();        \
    } while (false)
#endif

// ---------- 对外日志宏 ----------

// Engine侧(Console Logger)
#if defined(PHYSARA_DEBUG)
#define PHYSARA_CORE_TRACE(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetCoreLogger(), spdlog::level::trace, __VA_ARGS__)
#define PHYSARA_CORE_INFO(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetCoreLogger(), spdlog::level::info, __VA_ARGS__)
#else
#define PHYSARA_CORE_TRACE(...) ((void)0)
#define PHYSARA_CORE_INFO(...) ((void)0)
#endif

#define PHYSARA_CORE_WARN(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetCoreLogger(), spdlog::level::warn, __VA_ARGS__)
#define PHYSARA_CORE_ERROR(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetCoreLogger(), spdlog::level::err, __VA_ARGS__)
#define PHYSARA_CORE_FATAL(...)                                                                          \
    do                                                                                                   \
    {                                                                                                    \
        PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetCoreLogger(), spdlog::level::critical, __VA_ARGS__); \
        auto &_physara_core_logger_ref = ::Physara::Engine::Log::GetCoreLogger();                        \
        if (_physara_core_logger_ref)                                                                    \
        {                                                                                                \
            _physara_core_logger_ref->flush();                                                           \
        }                                                                                                \
        PHYSARA_FATAL_TRAP();                                                                            \
    } while (false)

// Editor侧(Windows Logger)
#if defined(PHYSARA_DEBUG)
#define PHYSARA_TRACE(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetEditorLogger(), spdlog::level::trace, __VA_ARGS__)
#define PHYSARA_INFO(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetEditorLogger(), spdlog::level::info, __VA_ARGS__)
#else
#define PHYSARA_TRACE(...) ((void)0)
#define PHYSARA_INFO(...) ((void)0)
#endif

#define PHYSARA_WARN(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetEditorLogger(), spdlog::level::warn, __VA_ARGS__)
#define PHYSARA_ERROR(...) PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetEditorLogger(), spdlog::level::err, __VA_ARGS__)
#define PHYSARA_FATAL(...)                                                                                 \
    do                                                                                                     \
    {                                                                                                      \
        PHYSARA_LOG_IMPL(::Physara::Engine::Log::GetEditorLogger(), spdlog::level::critical, __VA_ARGS__); \
        auto &_physara_editor_logger_ref = ::Physara::Engine::Log::GetEditorLogger();                      \
        if (_physara_editor_logger_ref)                                                                    \
        {                                                                                                  \
            _physara_editor_logger_ref->flush();                                                           \
        }                                                                                                  \
        PHYSARA_FATAL_TRAP();                                                                              \
    } while (false)