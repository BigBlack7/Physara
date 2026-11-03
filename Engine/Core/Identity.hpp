#pragma once

// 提供按领域隔离的值身份和不回绕的代号推进, 不管理对象存储或寿命.

#include <cstdint>
#include <limits>

namespace Physara::Core
{
    // generation=0 为无效身份. 标签只区分领域, 不承担资源查找或所有权.
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

    // 失败时不修改最后分配代, 不允许回绕到曾经发出的身份.
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
