#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <source_location>
#include <string_view>
#include <variant>
#include <vector>

#include "Error.hpp"

namespace Physara::Core
{
    namespace Detail
    {
        struct DiagnosticsChecks;
    }

    inline constexpr std::size_t DiagnosticCapacity = 1024;

    enum class Severity : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    enum class DiagnosticError : std::uint8_t
    {
        OutOfMemory,
        BackendFailure
    };

    struct DiagnosticFailure
    {
        DiagnosticError reason;
        ErrorContext context;
    };

    struct DiagnosticStats
    {
        std::size_t retained = 0;
        std::uint64_t evicted = 0;
        std::uint64_t truncated = 0;
        std::uint64_t writeFailed = 0;
    };

    struct DiagnosticRecord
    {
        Severity severity;
        BoundedText payload;
    };

    struct DiagnosticSnapshot
    {
        DiagnosticStats stats;
        std::vector<DiagnosticRecord> records;
    };

    // 多线程可写/读快照; 销毁前调用方必须收拢使用者. 不注册全局 logger.
    class Diagnostics final
    {
      public:
        using CreateResult = std::variant<std::unique_ptr<Diagnostics>, DiagnosticFailure>;
        using SnapshotResult = std::variant<DiagnosticSnapshot, DiagnosticFailure>;

        [[nodiscard]] static CreateResult Create() noexcept;
        ~Diagnostics();
        Diagnostics(const Diagnostics &) = delete;
        Diagnostics &operator=(const Diagnostics &) = delete;
        Diagnostics(Diagnostics &&) = delete;
        Diagnostics &operator=(Diagnostics &&) = delete;

        // payload 总计最多 2048 字节, 含单调秒时间戳和源位置; 失败返回 false.
        [[nodiscard]] bool Write(Severity severity, std::string_view message,
                                 std::source_location source = std::source_location::current()) noexcept;
        [[nodiscard]] DiagnosticStats GetStats() const;
        // 冷路径自持拷贝, 分配/复制成本与 limit 成正比, 最大 1024 条.
        [[nodiscard]] SnapshotResult ReadRecent(std::size_t limit = DiagnosticCapacity) const noexcept;

      private:
        struct Impl;
        explicit Diagnostics(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> m_Impl;
        friend struct Detail::DiagnosticsChecks;
    };
}
