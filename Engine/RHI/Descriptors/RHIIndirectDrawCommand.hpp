#pragma once

#include <cstdint>

namespace Physara::RHI
{
    struct RHIDrawIndexedIndirectCommand
    {
        std::uint32_t indexCount{0};
        std::uint32_t instanceCount{0};
        std::uint32_t firstIndex{0};
        std::int32_t vertexOffset{0};
        std::uint32_t firstInstance{0};
    };

    static_assert(sizeof(RHIDrawIndexedIndirectCommand) == sizeof(std::uint32_t) * 5u);
}