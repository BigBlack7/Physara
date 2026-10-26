#include "Diagnostics.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

#include <spdlog/details/circular_q.h>

#if defined(PHYSARA_CORE_NATIVE_CHECKS)
#include "DiagnosticsChecks.hpp"
#endif

namespace Physara::Core
{
    static Physara::Core::BoundedText BuildPayload(double seconds, std::string_view message,
                                                   const std::source_location &source) noexcept
    {
        Physara::Core::BoundedText payload("[+");
        std::array<char, 64> number{};
        const auto timeResult =
            std::to_chars(number.data(), number.data() + number.size(), seconds, std::chars_format::fixed, 6);
        if (timeResult.ec == std::errc{})
        {
            payload.Append({number.data(), static_cast<std::size_t>(timeResult.ptr - number.data())});
        }
        payload.Append(" s ");
        const std::string_view filename(source.file_name());
        const auto separator = filename.find_last_of("/\\");
        payload.Append(filename.substr(separator == std::string_view::npos ? 0 : separator + 1));
        payload.Append(":");
        const auto lineResult = std::to_chars(number.data(), number.data() + number.size(), source.line());
        if (lineResult.ec == std::errc{})
        {
            payload.Append({number.data(), static_cast<std::size_t>(lineResult.ptr - number.data())});
        }
        payload.Append("] ");
        payload.Append(message);
        return payload;
    }

    static Physara::Core::DiagnosticFailure MakeFailure(
        Physara::Core::DiagnosticError reason, std::string_view message,
        std::source_location source = std::source_location::current()) noexcept
    {
        return {reason, {Physara::Core::BoundedText(message), std::nullopt, source}};
    }
}

namespace Physara::Core
{
    struct Diagnostics::Impl
    {
        std::chrono::steady_clock::time_point origin = std::chrono::steady_clock::now();
        mutable std::mutex mutex;
        DiagnosticStats stats;
        spdlog::details::circular_q<DiagnosticRecord> records{DiagnosticCapacity};
#if defined(PHYSARA_CORE_NATIVE_CHECKS)
        bool failNextWrite = false;
#endif
    };

    Diagnostics::Diagnostics(std::unique_ptr<Impl> impl) noexcept : m_Impl(std::move(impl))
    {
    }

    Diagnostics::~Diagnostics() = default;

    Diagnostics::CreateResult Diagnostics::Create() noexcept
    {
        try
        {
            return std::unique_ptr<Diagnostics>(new Diagnostics(std::make_unique<Impl>()));
        }
        catch (const std::bad_alloc &)
        {
            return MakeFailure(DiagnosticError::OutOfMemory, "Diagnostics allocation failed");
        }
        catch (const std::exception &error)
        {
            return MakeFailure(DiagnosticError::BackendFailure, error.what());
        }
        catch (...)
        {
            return MakeFailure(DiagnosticError::BackendFailure, "Unknown diagnostics initialization failure");
        }
    }

    bool Diagnostics::Write(Severity severity, std::string_view message, std::source_location source) noexcept
    {
        try
        {
            std::scoped_lock lock(m_Impl->mutex);
            const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_Impl->origin);
            const auto payload = BuildPayload(seconds.count(), message, source);
            bool writeFailed = severity > Severity::Critical;
            try
            {
                if (!writeFailed)
                {
#if defined(PHYSARA_CORE_NATIVE_CHECKS)
                    if (m_Impl->failNextWrite)
                    {
                        m_Impl->failNextWrite = false;
                        throw std::bad_alloc();
                    }
#endif
                    m_Impl->records.push_back({severity, payload});
                }
            }
            catch (...)
            {
                writeFailed = true;
            }
            auto &stats = m_Impl->stats;
            if (writeFailed)
            {
                if (stats.writeFailed != std::numeric_limits<std::uint64_t>::max())
                {
                    ++stats.writeFailed;
                }
                return false;
            }
            if (stats.retained < DiagnosticCapacity)
            {
                ++stats.retained;
            }
            else if (stats.evicted != std::numeric_limits<std::uint64_t>::max())
            {
                ++stats.evicted;
            }
            if (payload.WasTruncated() && stats.truncated != std::numeric_limits<std::uint64_t>::max())
            {
                ++stats.truncated;
            }
            return true;
        }
        catch (...)
        {
            // 互斥量自身失败时无法安全访问计数, false 仍是可观察失败.
            return false;
        }
    }

    DiagnosticStats Diagnostics::GetStats() const
    {
        std::scoped_lock lock(m_Impl->mutex);
        return m_Impl->stats;
    }

    Diagnostics::SnapshotResult Diagnostics::ReadRecent(std::size_t limit) const noexcept
    {
        try
        {
            std::scoped_lock lock(m_Impl->mutex);
            // Debug STL 的默认构造和移动可能在 noexcept 内分配代理.
            // 使用可抛出的 count 构造, 返回时原位复制到结果, 保证 bad_alloc 在此边界捕获.
            DiagnosticSnapshot snapshot{m_Impl->stats, std::vector<DiagnosticRecord>(0)};
            const auto count = std::min(limit, snapshot.stats.retained);
            if (count > 0)
            {
                snapshot.records.reserve(count);
                const auto first = m_Impl->records.size() - count;
                for (std::size_t index = first; index < m_Impl->records.size(); ++index)
                {
                    snapshot.records.push_back(m_Impl->records.at(index));
                }
            }
            return SnapshotResult(std::in_place_type<DiagnosticSnapshot>, snapshot);
        }
        catch (const std::bad_alloc &)
        {
            return MakeFailure(DiagnosticError::OutOfMemory, "Diagnostic snapshot allocation failed");
        }
        catch (const std::exception &error)
        {
            return MakeFailure(DiagnosticError::BackendFailure, error.what());
        }
        catch (...)
        {
            return MakeFailure(DiagnosticError::BackendFailure, "Unknown diagnostic snapshot failure");
        }
    }

#if defined(PHYSARA_CORE_NATIVE_CHECKS)
    void Detail::DiagnosticsChecks::FailNextWrite(Diagnostics &diagnostics)
    {
        std::scoped_lock lock(diagnostics.m_Impl->mutex);
        diagnostics.m_Impl->failNextWrite = true;
    }

    void Detail::DiagnosticsChecks::SetLossCounters(Diagnostics &diagnostics, std::uint64_t value)
    {
        std::scoped_lock lock(diagnostics.m_Impl->mutex);
        diagnostics.m_Impl->stats.evicted = value;
        diagnostics.m_Impl->stats.truncated = value;
        diagnostics.m_Impl->stats.writeFailed = value;
    }
#endif
}
