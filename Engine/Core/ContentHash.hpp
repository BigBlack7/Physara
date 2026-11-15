#pragma once

/// @file
/// @brief 对原始字节计算 SHA-256, 供资产快照和 Shader 包共用.

#include <array>
#include <cstddef>
#include <span>
#include <variant>

namespace Physara::Core
{
    using Sha256Digest = std::array<unsigned char, 32>;

    enum class Sha256Error
    {
        InputTooLarge,
        InitializationFailed,
        ComputationFailed
    };

    /// @brief 计算输入原始字节的摘要, 不变更编码或字节顺序.
    /// @return 摘要或 CNG 失败类别. 分配失败由调用者的错误边界处理.
    /// @note 输入仅在调用期间借用, 空输入也有有效摘要; 每次调用独占 CNG 状态, 可并发使用.
    [[nodiscard]] std::variant<Sha256Digest, Sha256Error> ComputeSha256(std::span<const std::byte> bytes);
}
