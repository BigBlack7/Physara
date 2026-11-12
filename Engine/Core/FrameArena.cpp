#include "FrameArena.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <new>
#include <utility>

/// @file
/// @brief 在发布任何范围前完成扩容, 用剩余容量限制对齐与元素乘法.

namespace Physara::Core
{
    // 在发布首段借用前换入新块. 容量上限约束 bit_ceil 的输入和舍入结果.
    bool FrameArena::Reserve(std::size_t bytes) noexcept
    {
        if (bytes <= m_Capacity)
        {
            return true;
        }
        if (bytes > MaxBytes || m_Used != 0)
        {
            return false;
        }
        const auto capacity = std::bit_ceil(std::max(bytes, MaxAlignment));
        std::unique_ptr<std::byte[]> storage(new (std::nothrow) std::byte[capacity]);
        if (!storage)
        {
            return false;
        }
        m_Storage = std::move(storage);
        m_Capacity = capacity;
        return true;
    }

    // 所有检查通过后一次推进游标和高水位, 失败时已分配的区段保持原地址.
    std::span<std::byte> FrameArena::Allocate(std::size_t count, std::size_t elementSize,
                                             std::size_t alignment) noexcept
    {
        if (count == 0 || elementSize == 0 || !std::has_single_bit(alignment) || alignment > MaxAlignment)
        {
            return {};
        }
        // 字节数组起始地址满足基础对齐. 先扣除填充, 再按可用容量核对乘积.
        const auto remainder = m_Used & (alignment - 1);
        const auto padding = (alignment - remainder) & (alignment - 1);
        const auto available = m_Capacity - m_Used;
        if (padding > available || count > (available - padding) / elementSize)
        {
            return {};
        }
        const auto start = m_Used + padding;
        const auto size = count * elementSize;
        m_Used = start + size;
        m_HighWater = std::max(m_HighWater, m_Used);
        return {m_Storage.get() + start, size};
    }

    void FrameArena::Rewind(std::size_t used) noexcept
    {
        assert(used <= m_Used);
        m_Used = used;
    }
}
