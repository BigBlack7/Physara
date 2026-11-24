#pragma once

/// @file
/// @brief 当前 Forward profile 的原生准备键和有界需求. 源角色和固定状态选择共同标识一个槽.

#include <array>
#include <bitset>
#include <filesystem>
#include <span>

#include <Shader/Reload.hpp>
#include <Shader/RhiProgram.hpp>

namespace Physara::Renderer::Detail
{
    // 每张表按 8 种顶点布局, 4 种绕序/剔面组合及 4 个表面阶段编址. 两个深度角色共用首张表.
    // 设备计数上界还涵盖天空和四个显示阶段的两种目标格式.
    inline constexpr std::size_t GeometryPhaseSlots = 8 * 4;
    inline constexpr std::size_t GeometryPipelineSlots = GeometryPhaseSlots * 4;
    inline constexpr std::size_t ForwardColorPrograms = 4;
    inline constexpr std::size_t ForwardPipelineLimit = GeometryPipelineSlots * ForwardColorPrograms + 1 + 8;

    /// @brief role 选择程序, variant 选择该程序的固定状态. 天空用槽零, 显示用线性或 sRGB 附件槽.
    struct ShaderPipelineRequest
    {
        Shader::ProductShader role;
        std::size_t variant;
    };

    /// @brief 用位集汇总各 View 所需的不同原生键. 同一键可由多个 Draw 或 View 重复登记.
    class ShaderDemand final
    {
    public:
        void Add(ShaderPipelineRequest key) noexcept;
        [[nodiscard]] bool Contains(ShaderPipelineRequest key) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept;
    private:
        std::array<std::bitset<GeometryPipelineSlots>, Shader::ProductShaderCount> m_Keys;
    };

    /// @brief 按已知产品角色消费打包或后台已验证的包, 两条路径使用同一 ABI 和阶段检查.
    [[nodiscard]] Shader::Result<Shader::RhiProgram> LoadProductProgram(RHI::Device &device,
        Shader::ProductShader role, const std::filesystem::path &path, const Shader::ProductPackages &packages,
        std::string_view abi, std::span<const Shader::ExpectedActiveResources> expected = {});
}
