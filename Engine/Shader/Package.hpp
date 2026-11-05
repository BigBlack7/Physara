#pragma once

// 版本化包的 CPU 文件边界, 不把 Engine 包类型传给公开 RHI.

#include <filesystem>
#include <string_view>

#include "Types.hpp"

namespace Physara::Shader
{
    // 调用期间借用预期 ABI. 从完整声明快照重算 ABI, 核对 IR/元数据与消费者所需身份后交付拥有值.
    [[nodiscard]] Result<ShaderPackage> ReadPackage(const std::filesystem::path &path,
                                                    std::string_view expectedAbiHash) noexcept;

    // 原子替换同目录文件. 成功 true 表示内容改变, false 表示旧文件内容完全相同.
    // 失败保留旧完整包并返回 Failure; 路径与包只在调用期间借用.
    [[nodiscard]] Result<bool> WritePackage(const std::filesystem::path &path, const ShaderPackage &package) noexcept;
}
