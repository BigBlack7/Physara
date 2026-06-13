#pragma once

#include <cstdint>
#include <limits>

namespace Physara::Engine
{
    using MaterialInstanceId = std::uint32_t;

    inline constexpr MaterialInstanceId InvalidMaterialInstanceId = std::numeric_limits<MaterialInstanceId>::max();
}