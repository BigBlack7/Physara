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
    /// @note names 按唯一名称列出该角色的资源, 顺序任意. 文件和内存入口都只在本次调用期间借用它.
    struct ExpectedActiveResources
    {
        Stage stage;
        std::uint32_t set;
        std::span<const std::string_view> names;
    };

    /// @brief 在已验证的包中逐组比对指定阶段和 set 的全部活动资源名. 空配方跳过此项检查.
    /// @return 所有指定组都匹配时返回空, 首个不匹配返回 InterfaceMismatch. 此检查只读取 CPU 反射.
    [[nodiscard]] std::optional<Failure> CheckActiveResources(const ShaderPackage &package,
        std::span<const ExpectedActiveResources> expected) noexcept;

    /// @brief 校验包及 required ABI 后创建两阶段 Shader, 任一失败不交付部分集合.
    /// @param expectedAbiHash 消费者所需的接口身份, 通常取自生成 Data.hpp 的 AbiHash.
    /// @param expectedActive 需要区分同 ABI 的角色时传入. 指定阶段和集合的活动资源必须恰好匹配,
    /// 否则在创建 RHI Shader 前返回 InterfaceMismatch.
    /// @param expectedFragmentSource 可选的包内逻辑片元源. 算法使用同一布局时, 消费者用它确认实际阶段.
    /// @return 自持 VS/PS 与包身份, 或包校验/工厂失败; 先前创建的调用方对象不受影响.
    /// @note device 借用到返回, 原生后端的线程/context 前提由调用方满足; 结果保活各自 RHI 依赖.
    [[nodiscard]] Result<RhiProgram> LoadRhiProgram(RHI::Device &device, const std::filesystem::path &path,
                                                    std::string_view expectedAbiHash,
                                                    std::optional<ExpectedActiveResources> expectedActive = {},
                                                    std::string_view expectedFragmentSource = {}) noexcept;
    /// @brief 从编译或读取已验证的包取得同一图形阶段集合, 核对调用方所需 ABI 与角色.
    /// @pre package 来自成功的 Compiler 或 ReadPackage, device 满足创建 Shader 的线程及 context 合同.
    /// @param expectedActive 可同时约束多个绑定集合. 各组及其资源名只借用到返回.
    /// @note 源参数使用包内逻辑路径. 返回值持有两阶段 Shader, 包及期望资源名的存储可在返回后释放.
    [[nodiscard]] Result<RhiProgram> CreateRhiProgram(RHI::Device &device, const ShaderPackage &package,
        std::string_view expectedAbiHash, std::span<const ExpectedActiveResources> expectedActive = {},
        std::string_view expectedFragmentSource = {}, std::string_view expectedVertexSource = {}) noexcept;
    /// @brief 完整计算模块与所验证的包身份, 阶段对象由调用方持有.
    struct RhiComputeProgram
    {
        RHI::ShaderHandle compute;
        std::array<std::uint32_t, 3> localSize{};
        std::string abiHash;
        std::string contentHash;
    };

    /// @brief 验证单阶段 Compute 包的 ABI 和 profile 后创建设备 Shader.
    /// @return 拥有 CS, 固定工作组尺寸及包身份的结果. 读取或创建失败时返回 Failure.
    /// @pre 在设备允许创建 Shader 的线程及 context 调用.
    [[nodiscard]] Result<RhiComputeProgram> LoadRhiComputeProgram(RHI::Device &device,
        const std::filesystem::path &path, std::string_view expectedAbiHash) noexcept;
} // namespace Physara::Shader
