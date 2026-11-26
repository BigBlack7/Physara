#pragma once

/// @file
/// @brief 产品几何, 表面 MRT, 阴影和显示程序目录. 构建驱动及热重载按同一角色取得变体和源入口.

#include <span>

#include "Types.hpp"

namespace Physara::Shader
{
    /// @brief 产品包数组的稳定索引. Count 同时表示诊断尚未定位到某个角色.
    enum class ProductShader : std::uint8_t
    {
        StaticForward,
        StaticDepth,
        ForwardDisplay,
        StaticForwardPhysical,
        StaticForwardIbl,
        ForwardSky,
        DisplayColorGrading,
        DisplayFxaa,
        DisplayScale,
        StaticShadowDepth,
        StaticCascadePreview,
        StaticSpotShadowPreview,
        StaticPointShadowPreview,
        StaticForwardShadows,
        StaticForwardPhysicalShadows,
        StaticForwardIblShadows,
        StaticForwardPlus,
        StaticForwardPlusPhysical,
        StaticForwardPlusIbl,
        StaticForwardPlusShadows,
        StaticForwardPlusPhysicalShadows,
        StaticForwardPlusIblShadows,
        StaticCoreGBuffer,
        StaticSurfaceAux,
        Count
    };

    inline constexpr std::size_t ProductShaderCount = static_cast<std::size_t>(ProductShader::Count);
    inline constexpr std::string_view PhysicalLightingField = "PHYSARA_PHYSICAL_LIGHTING";

    /// @brief 固定产品角色及唯一获准的结构值. 零表示该角色未声明物理光照 define.
    struct VariantKey
    {
        ProductShader role = ProductShader::Count;
        std::uint32_t physicalLighting = 0;
        bool operator==(const VariantKey &) const noexcept = default;
    };

    struct VariantField
    {
        std::string_view name;
        std::uint32_t value = 0;
    };

    /// @brief 路径均为 Shaders 根下的相对路径. 两个图形阶段沿公开 canonical profile 的 main 入口.
    struct ProductProgram
    {
        VariantKey key;
        std::string_view name;
        std::string_view interfaceName;
        std::string_view request;
        std::string_view vertex;
        std::string_view fragment;
    };

    /// @brief 返回进程寿命内的只读目录. 两个 Find 重载在角色或名称缺席时返回空指针.
    [[nodiscard]] std::span<const ProductProgram, ProductShaderCount> ProductPrograms() noexcept;
    [[nodiscard]] const ProductProgram *FindProductProgram(ProductShader role) noexcept;
    [[nodiscard]] const ProductProgram *FindProductProgram(std::string_view name) noexcept;
    /// @brief 核对外部字段与该角色的完整组合. 仅返回目录中的键, 拒绝未声明或重复的字段.
    [[nodiscard]] Result<VariantKey> ResolveVariant(
        std::string_view program, std::span<const VariantField> fields) noexcept;
}
