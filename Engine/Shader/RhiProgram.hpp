#pragma once

/// @file
/// @brief 已验证包到公共 RHI Shader 的薄消费入口, 不参与 GL lowering 或设备所有权.

#include <filesystem>
#include <optional>
#include <span>
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

    /// @brief 同一 ABI 的程序变体在指定阶段、集合内实际需要的全部资源名.
    /// @note names 按唯一名称列出该角色的资源, 顺序任意. 名称和数组仅在 LoadRhiProgram 调用期间借用.
    struct ExpectedActiveResources
    {
        Stage stage;
        std::uint32_t set;
        std::span<const std::string_view> names;
    };

    /// @brief 校验包及 required ABI 后创建两阶段 Shader, 任一失败不交付部分集合.
    /// @param expectedAbiHash 消费者所需的接口身份, 通常取自生成 Data.hpp 的 AbiHash.
    /// @param expectedActive 需要区分同 ABI 的角色时传入. 指定阶段和集合的活动资源必须恰好匹配,
    /// 否则在创建 RHI Shader 前返回 InterfaceMismatch.
    /// @return 自持 VS/PS 与包身份, 或包校验/工厂失败; 先前创建的调用方对象不受影响.
    /// @note device 借用到返回, 原生后端的线程/context 前提由调用方满足; 结果保活各自 RHI 依赖.
    [[nodiscard]] Result<RhiProgram> LoadRhiProgram(RHI::Device &device, const std::filesystem::path &path,
                                                    std::string_view expectedAbiHash,
                                                    std::optional<ExpectedActiveResources> expectedActive = {}) noexcept;
} // namespace Physara::Shader
