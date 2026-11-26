#pragma once

/// @file
/// @brief 当前 Forward profile 的原生准备键和有界需求. 源角色和固定状态选择共同标识一个槽.

#include <array>
#include <bitset>
#include <filesystem>
#include <span>
#include <variant>

#include <Shader/Reload.hpp>
#include <Shader/RhiProgram.hpp>

namespace Physara::Renderer::Detail
{
    // 每张表按 8 种顶点布局, 4 种绕序/剔面组合及 4 个表面阶段编址. 两个深度角色共用首张表.
    // 核心及辅助表面各用一个深度合写的阶段, 在常规表尾分别保有 32 个槽.
    // 设备计数上界还涵盖天空和四个显示阶段的两种目标格式.
    inline constexpr std::size_t GeometryPhaseSlots = 8 * 4;
    inline constexpr std::size_t GeometryPipelineSlots = GeometryPhaseSlots * 4;
    // 前九项各有 set 3 配方. 后六项使用 Froxel set 0, 同一受光组合复用前面的 set 3.
    inline constexpr std::size_t ForwardPerPassLayouts = 9;
    inline constexpr std::size_t ForwardColorPrograms = ForwardPerPassLayouts + 6;
    inline constexpr std::size_t CorePipelineBase = GeometryPipelineSlots * ForwardColorPrograms;
    inline constexpr std::size_t SurfaceAuxPipelineBase = CorePipelineBase + GeometryPhaseSlots;
    inline constexpr std::size_t GeometryPipelineCount = SurfaceAuxPipelineBase + GeometryPhaseSlots;
    inline constexpr std::size_t ForwardPipelineLimit = GeometryPipelineCount + 1 + 8;

    /// @brief 各表面角色按物理颜色位置列出逻辑 GBuffer 下标. 图写入及目标配方沿用同一顺序.
    inline constexpr std::array<std::size_t, 4> CoreSurfaceAttachments{0, 3, 2, 1};
    inline constexpr std::array<std::size_t, 2> AuxSurfaceAttachments{1, 3};

    /// @brief 返回按原生输出位置排列的逻辑附件下标. 空范围表示该角色使用自身的普通颜色或深度配方.
    [[nodiscard]] constexpr std::span<const std::size_t> SurfaceAttachments(Shader::ProductShader role) noexcept
    {
        switch (role)
        {
        case Shader::ProductShader::StaticCoreGBuffer: return CoreSurfaceAttachments;
        case Shader::ProductShader::StaticSurfaceAux: return AuxSurfaceAttachments;
        default: return {};
        }
    }

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
        /// @brief 仍可执行的计划优先于待选颜色. 同一键也被有效计划要求时保留其优先级.
        void Add(ShaderPipelineRequest key, bool required = true) noexcept;
        [[nodiscard]] bool Contains(ShaderPipelineRequest key) const noexcept;
        [[nodiscard]] bool Required(ShaderPipelineRequest key) const noexcept;
        /// @brief 该角色还有任一固定状态键被请求时, 准备器可继续保留这份包的拒绝原因.
        [[nodiscard]] bool ContainsRole(Shader::ProductShader role) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept;
    private:
        // m_Keys 用于整代就绪判断, m_Required 是其中享有本轮原生创建优先级的子集.
        std::array<std::bitset<GeometryPipelineSlots>, Shader::ProductShaderCount> m_Keys;
        std::array<std::bitset<GeometryPipelineSlots>, Shader::ProductShaderCount> m_Required;
    };

    /// @brief 保有读包, 接口校验或设备创建的首因. 失败处同时决定它对其它准备工作的影响.
    struct ProductProgramFailure
    {
        Shader::Failure cause;
        bool rejectsRole; ///< 为真时活动代跳过同包的所有固定状态键, 继续准备其它角色. 为假时结束本次推进.
    };
    using ProductProgramResult = std::variant<Shader::RhiProgram, ProductProgramFailure>;

    /// @brief 按已知产品角色消费打包或后台已验证的包, 两条路径使用同一 ABI 和阶段检查.
    [[nodiscard]] ProductProgramResult LoadProductProgram(RHI::Device &device,
        Shader::ProductShader role, const std::filesystem::path &path, const Shader::ProductPackages &packages,
        std::string_view abi, std::span<const Shader::ExpectedActiveResources> expected = {});

    /// @brief 保留产品角色与加载首因. 包级准入拒绝可由活动代跳过整个角色, 设备错误结束本次推进.
    [[noreturn]] void RejectProductProgram(std::string_view roleName, const ProductProgramFailure &failure);
}
