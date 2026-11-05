#pragma once

/// @file
/// @brief 最终 IR 的验证与反射核对入口, 编译与包读取共享同一合同.

#include <span>

#include "ShaderSchema.hpp"

namespace Physara::Shader
{
    /// @brief 检查 SPIR-V 1.3 头与预算, 再用 Vulkan 1.1 validator 核对完整字节码.
    /// @throws BuildError 输入或验证失败, 借用不跨返回保留.
    void ValidateSpirv(std::span<const std::uint32_t> words);
    /// @brief 验证最终模块的 profile 与 ABI, 全部成功后才替换活动反射.
    void ReflectModule(ShaderModule &module, const InterfaceSchema &schema);
    /// @brief 核对 VS/FS 活动 IO 衔接与同一 set/binding 的跨阶段资源一致性.
    void CheckProgramInterfaces(const std::vector<ShaderModule> &modules);
}
