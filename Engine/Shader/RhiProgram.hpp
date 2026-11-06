#pragma once

/// @file
/// @brief 已验证包到公共 RHI Shader 的薄消费入口, 不参与 GL lowering 或设备所有权.

#include <filesystem>
#include <string>
#include <string_view>

#include <RHI.hpp>

#include "Types.hpp"

namespace Physara::Shader
{
    /// @brief 完整图形阶段集合及其包身份, handles 各自拥有字节码和设备依赖.
    /// @note abiHash 标识接口, contentHash 标识本次包输入; 两者不代表后端 Program 已链接或可执行 Draw.
    struct RhiProgram
    {
        RHI::ShaderHandle vertex;
        RHI::ShaderHandle fragment;
        std::string abiHash;
        std::string contentHash;
    };

    /// @brief 校验包及 required ABI 后创建两阶段 Shader, 任一失败不交付部分集合.
    /// @param expectedAbiHash 消费者所需的接口身份, 通常取自生成 Data.hpp 的 AbiHash.
    /// @return 自持 VS/PS 与包身份, 或包校验/工厂失败; 先前创建的调用方对象不受影响.
    /// @note device 借用到返回, 原生后端的线程/context 前提由调用方满足; 结果保活各自 RHI 依赖.
    [[nodiscard]] Result<RhiProgram> LoadRhiProgram(RHI::Device &device, const std::filesystem::path &path,
                                                    std::string_view expectedAbiHash) noexcept;
} // namespace Physara::Shader
