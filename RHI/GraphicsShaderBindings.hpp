#pragma once

/// @file
/// @brief 产品换代使用的设备绑定预检. 只消费已创建的两个图形 Shader 和实际逻辑布局.

#include <optional>
#include <string>

#include "RHI.hpp"

namespace Physara::RHI
{
    /// @brief 由设备复用图形 Pipeline 的绑定规划, 不创建 Program 或选择固定光栅状态.
    class GraphicsShaderBindingPreparation
    {
    public:
        /// @return 空表示匹配当前设备的绑定合同, 否则携带可定位的首个拒绝原因.
        /// @note 在设备 owner 线程短借所有对象; 不修改设备状态.
        [[nodiscard]] virtual std::optional<std::string> CheckGraphicsShaderBindings(
            Shader &vertex, Shader &fragment, const BindingLayoutVector &layouts) const = 0;

    protected:
        ~GraphicsShaderBindingPreparation() = default;
    };

    /// @brief 后端没有实现本项准备时明确拒绝. 普通 Pipeline 工厂仍沿各自的合同工作.
    [[nodiscard]] inline std::optional<std::string> CheckGraphicsShaderBindings(Device &device,
        Shader &vertex, Shader &fragment, const BindingLayoutVector &layouts)
    {
        const auto *preparation = dynamic_cast<const GraphicsShaderBindingPreparation *>(&device);
        if (!preparation)
        {
            return "Graphics Shader binding preparation is unavailable on this device";
        }
        return preparation->CheckGraphicsShaderBindings(vertex, fragment, layouts);
    }
}
