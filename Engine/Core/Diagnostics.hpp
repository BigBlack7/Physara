#pragma once

/// @file
/// @brief 有界 CPU 诊断记录及自持快照接口, 与窗口和图形设备的初始化独立.

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

    /// @brief 诊断设施自身的失败, 与调用方正在报告的领域错误分开传递.
    struct DiagnosticFailure
    {
        DiagnosticError reason;
        ErrorContext context;
    };

    /// @brief 环中保留量与累计损失, 累计计数达到 uint64_t 上限后饱和.
    struct DiagnosticStats
    {
        std::size_t retained = 0;
        std::uint64_t evicted = 0;    ///< 容量已满后被成功写入挤出的旧记录数.
        std::uint64_t truncated = 0;  ///< 成功入环但正文被截断的记录数.
        std::uint64_t writeFailed = 0; ///< 可在锁内统计的失败次数, 不含互斥量本身失败.
    };

    /// @brief 一条自持诊断, payload 已包含相对时间, 文件名和行号.
    struct DiagnosticRecord
    {
        Severity severity;
        BoundedText payload;
    };

    /// @brief 同一次加锁取得的计数与最近记录, 返回后不依赖 Diagnostics 的寿命.
    struct DiagnosticSnapshot
    {
        // records 从旧到新排列; limit 只限制本次拷贝, 不影响统计或环中内容.
        DiagnosticStats stats;
        std::vector<DiagnosticRecord> records;
    };

    /// @brief 诊断环与统计的唯一 owner, 不注册全局 logger.
    /// @par 线程与寿命
    /// 写入和读取相互同步; 销毁前调用方必须收拢全部使用者.
    class Diagnostics final
    {
      public:
        using CreateResult = std::variant<std::unique_ptr<Diagnostics>, DiagnosticFailure>;
        using SnapshotResult = std::variant<DiagnosticSnapshot, DiagnosticFailure>;

        /// @brief 创建固定容量诊断环; 初始化或分配失败返回 DiagnosticFailure.
        [[nodiscard]] static CreateResult Create() noexcept;
        ~Diagnostics();
        Diagnostics(const Diagnostics &) = delete;
        Diagnostics &operator=(const Diagnostics &) = delete;
        Diagnostics(Diagnostics &&) = delete;
        Diagnostics &operator=(Diagnostics &&) = delete;

        /// @brief 同步拷贝消息入环, payload 总计最多 2048 字节, 含单调秒时间戳和源位置.
        /// @return 入环成功为 true; 非法级别或写入/加锁失败为 false, 不替代调用方的领域错误.
        [[nodiscard]] bool Write(Severity severity, std::string_view message,
                                 std::source_location source = std::source_location::current()) noexcept;
        /// @brief 同步复制计数, 不拷贝正文; 互斥量自身的标准库异常可传播.
        [[nodiscard]] DiagnosticStats GetStats() const;
        /// @brief 从旧到新拷贝最近 min(limit, retained) 条记录, 最多 1024 条.
        /// @return 自持快照或 DiagnosticFailure; 失败不消费或修改环中记录.
        [[nodiscard]] SnapshotResult ReadRecent(std::size_t limit = DiagnosticCapacity) const noexcept;

      private:
        struct Impl;
        explicit Diagnostics(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> m_Impl;
    };
}
