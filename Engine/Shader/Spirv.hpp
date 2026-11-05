#pragma once

// 最终 IR 的唯一核对入口, 同时用于正常编译与独立 ABI 负例. 不提供跳过 validator 的路径.

#include <span>

#include "ShaderSchema.hpp"

namespace Physara::Shader
{
    void ValidateSpirv(std::span<const std::uint32_t> words);
    void ReflectModule(ShaderModule &module, const InterfaceSchema &schema);
    void CheckProgramInterfaces(const std::vector<ShaderModule> &modules);
}
