#pragma once

#include <glm/vec3.hpp>

namespace Physara::FilamentDisplay
{
    /// @brief 按固定 Filament 色调算子和目标色域映射处理非负预曝光 Rec.709.
    [[nodiscard]] glm::vec3 GradeSdr(glm::vec3 rec709) noexcept;
}
