#pragma once

/// @file
/// @brief 版本化 Shader 包的 CPU 文件边界, 独立核对声明与 IR, 不创建 RHI 对象.

#include <filesystem>
#include <string_view>

#include "Types.hpp"

namespace Physara::Shader
{
    /// @brief 读取并重算声明 ABI, 核对最终 IR, 元数据与文件完整性后交付自持值.
    /// @param expectedAbiHash 消费者期望的接口 ABI, 必须与重算结果一致.
    /// @return 完整 ShaderPackage 或 Failure, 不交付部分校验结果.
    [[nodiscard]] Result<ShaderPackage> ReadPackage(const std::filesystem::path &path,
                                                    std::string_view expectedAbiHash) noexcept;

    /// @brief 按包写入规则验证后返回头部, 元数据, 对齐填充及模块载荷的总字节数, 供 CPU 缓存准入.
    /// @return 编码长度或验证及分配失败. 调用方仍持有原包.
    [[nodiscard]] Result<std::size_t> MeasurePackage(const ShaderPackage &package) noexcept;

    /// @brief 通过同目录临时文件原子发布 Shader 包.
    /// @return true 表示内容改变, false 表示与旧文件相同; 失败返回 Failure.
    /// @note 发布失败时保留旧包, 输入仅在调用期间借用.
    [[nodiscard]] Result<bool> WritePackage(const std::filesystem::path &path, const ShaderPackage &package) noexcept;
}
