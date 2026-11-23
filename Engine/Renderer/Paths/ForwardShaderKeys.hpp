#pragma once

/// @file
/// @brief 当前 Forward profile 的原生准备键和有界需求. 源角色和固定状态选择共同标识一个槽.

#include <array>
#include <bitset>
#include <filesystem>

#include <Shader/Reload.hpp>
#include <Shader/RhiProgram.hpp>

namespace Physara::Renderer::Detail
{
    // 几何表按八种顶点布局, 四种镜像和双面组合, 三种表面阶段预留槽. 深度角色借用首表的深度区间.
    // 设备额度按三张几何表, 单个天空槽和四个显示阶段的两种附件格式取上界.
    inline constexpr std::size_t GeometryPipelineSlots = 8 * 4 * 3;
    inline constexpr std::size_t ForwardPipelineLimit = GeometryPipelineSlots * 3 + 1 + 8;

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
        std::string_view abi, std::optional<Shader::ExpectedActiveResources> expected = {});
}
