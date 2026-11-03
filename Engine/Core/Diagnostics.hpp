#pragma once

// 有界 CPU 诊断记录的唯一 owner; 写入可并发, 消费者通过自持快照读取最近记录和损失计数.

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
        // stats 和 records 在同一把锁内取得, records 从旧到新排列; 返回后不依赖诊断 owner 的寿命.
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
        // 只复制计数; 与 Write/ReadRecent 同步, 锁自身的标准库异常可传播.
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
