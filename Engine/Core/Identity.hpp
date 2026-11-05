#pragma once

/// @file
/// @brief 提供按领域隔离的值身份和不回绕的代号推进, 不管理对象存储或寿命.

#include <cstdint>
#include <limits>

namespace Physara::Core
{
    /// @brief 由槽索引与分配代组成的领域身份, generation=0 表示无效.
    /// @tparam Tag 仅隔离不同领域的身份类型, 不承担资源查找或所有权.
    template <typename Tag>
    struct Identity
    {
        std::uint32_t index = 0;
        std::uint64_t generation = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return generation != 0;
        }

        bool operator==(const Identity &) const = default;
    };

    /// @brief 分配下一个非零代号, 禁止回绕后复用历史身份.
    /// @return 耗尽时返回 false 并保持 lastGeneration, 成功时递增一次.
    [[nodiscard]] constexpr bool AdvanceGeneration(std::uint64_t &lastGeneration) noexcept
    {
        if (lastGeneration == std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }
        ++lastGeneration;
        return true;
    }
}
