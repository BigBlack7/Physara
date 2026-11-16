#pragma once

/// @file
/// @brief 跨 worker 和只读版本共享的字节额度. 分配/释放内存的 owner 负责成对记账.

#include <atomic>
#include <cassert>
#include <cstddef>

namespace Physara::Asset
{
    /// @brief 候选、当前版本和旧读者共享的容量, 寿命可超过 AssetManager.
    /// 不拥有载荷, 不等待容量变为可用; 最后一个载荷 owner 销毁时才可归还相应额度.
    class ByteBudget final
    {
    public:
        explicit ByteBudget(std::size_t limit) noexcept : m_Limit(limit) {}

        /// @brief 分配前先取得额度. 这里只同步计数, 数据可见性由作业交接提供.
        [[nodiscard]] bool TryReserve(std::size_t bytes) noexcept
        {
            auto used = m_Used.load(std::memory_order_relaxed);
            do
            {
                if (bytes > m_Limit - used)
                {
                    return false;
                }
            } while (!m_Used.compare_exchange_weak(used, used + bytes, std::memory_order_relaxed));
            return true;
        }

        /// @brief 仅由持有额度的唯一 owner 退还已取得的容量.
        void Release(std::size_t bytes) noexcept
        {
            [[maybe_unused]] const auto previous = m_Used.fetch_sub(bytes, std::memory_order_relaxed);
            assert(previous >= bytes);
        }

    private:
        const std::size_t m_Limit;
        std::atomic<std::size_t> m_Used{0};
    };
}
