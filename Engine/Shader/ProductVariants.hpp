#pragma once

/// @file
/// @brief 产品程序目录涵盖几何, 表面 MRT 及预览, 屏幕照明, 阴影和显示. 构建驱动及热重载共用角色和源入口.

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
        DeferredLighting,
        DeferredLightingPhysical,
        DeferredLightingIbl,
        DeferredLightingShadows,
        DeferredLightingPhysicalShadows,
        DeferredLightingIblShadows,
        SurfaceDepthPreview,
        SurfaceGBufferPreview,
        StaticForwardIorSpecular,
        StaticForwardIorSpecularPhysical,
        StaticForwardIorSpecularIbl,
        StaticForwardIorSpecularShadows,
        StaticForwardIorSpecularPhysicalShadows,
        StaticForwardIorSpecularIblShadows,
        StaticForwardPlusIorSpecular,
        StaticForwardPlusIorSpecularPhysical,
        StaticForwardPlusIorSpecularIbl,
        StaticForwardPlusIorSpecularShadows,
        StaticForwardPlusIorSpecularPhysicalShadows,
        StaticForwardPlusIorSpecularIblShadows,
        StaticForwardClearcoat,
        StaticForwardClearcoatPhysical,
        StaticForwardClearcoatIbl,
        StaticForwardClearcoatShadows,
        StaticForwardClearcoatPhysicalShadows,
        StaticForwardClearcoatIblShadows,
        StaticForwardPlusClearcoat,
        StaticForwardPlusClearcoatPhysical,
        StaticForwardPlusClearcoatIbl,
        StaticForwardPlusClearcoatShadows,
        StaticForwardPlusClearcoatPhysicalShadows,
        StaticForwardPlusClearcoatIblShadows,
        StaticForwardSheen,
        StaticForwardSheenPhysical,
        StaticForwardSheenIbl,
        StaticForwardSheenShadows,
        StaticForwardSheenPhysicalShadows,
        StaticForwardSheenIblShadows,
        StaticForwardPlusSheen,
        StaticForwardPlusSheenPhysical,
        StaticForwardPlusSheenIbl,
        StaticForwardPlusSheenShadows,
        StaticForwardPlusSheenPhysicalShadows,
        StaticForwardPlusSheenIblShadows,
        StaticForwardSheenClearcoat,
        StaticForwardSheenClearcoatPhysical,
        StaticForwardSheenClearcoatIbl,
        StaticForwardSheenClearcoatShadows,
        StaticForwardSheenClearcoatPhysicalShadows,
        StaticForwardSheenClearcoatIblShadows,
        StaticForwardPlusSheenClearcoat,
        StaticForwardPlusSheenClearcoatPhysical,
        StaticForwardPlusSheenClearcoatIbl,
        StaticForwardPlusSheenClearcoatShadows,
        StaticForwardPlusSheenClearcoatPhysicalShadows,
        StaticForwardPlusSheenClearcoatIblShadows,
        Count
    };

    inline constexpr std::size_t ProductShaderCount = static_cast<std::size_t>(ProductShader::Count);
    inline constexpr std::string_view PhysicalLightingField = "PHYSARA_PHYSICAL_LIGHTING";
    inline constexpr std::string_view IorSpecularField = "PHYSARA_IOR_SPECULAR";
    inline constexpr std::string_view ClearcoatField = "PHYSARA_CLEARCOAT";
    inline constexpr std::string_view SheenField = "PHYSARA_SHEEN";
    inline constexpr std::size_t IorSpecularProgramCount =
        std::size_t(ProductShader::StaticForwardPlusIorSpecularIblShadows) -
        std::size_t(ProductShader::StaticForwardIorSpecular) + 1;

    [[nodiscard]] constexpr bool IsIorSpecularProgram(ProductShader role) noexcept
    {
        return role >= ProductShader::StaticForwardIorSpecular &&
            role <= ProductShader::StaticForwardPlusIorSpecularIblShadows;
    }

    /// @brief 角色按全灯/Froxel 分组, 每组为 Direct/Physical/IBL 及同序三个带阴影角色.
    /// @pre IsIorSpecularProgram(role) 成立. 返回值用于对应组的产品路径和原生颜色程序表.
    [[nodiscard]] constexpr std::size_t IorSpecularProgramIndex(ProductShader role) noexcept
    {
        return std::size_t(role) - std::size_t(ProductShader::StaticForwardIorSpecular);
    }

    inline constexpr std::size_t ClearcoatProgramCount =
        std::size_t(ProductShader::StaticForwardPlusClearcoatIblShadows) -
        std::size_t(ProductShader::StaticForwardClearcoat) + 1;
    static_assert(ClearcoatProgramCount == IorSpecularProgramCount);

    [[nodiscard]] constexpr bool IsClearcoatProgram(ProductShader role) noexcept
    {
        return role >= ProductShader::StaticForwardClearcoat && role <= ProductShader::StaticForwardPlusClearcoatIblShadows;
    }

    /// @brief 按与 IOR/specular 相同的全灯/Froxel 受光顺序索引清漆颜色程序.
    /// @pre IsClearcoatProgram(role) 成立.
    [[nodiscard]] constexpr std::size_t ClearcoatProgramIndex(ProductShader role) noexcept
    {
        return std::size_t(role) - std::size_t(ProductShader::StaticForwardClearcoat);
    }

    inline constexpr std::size_t SheenProgramCount = std::size_t(ProductShader::Count) -
        std::size_t(ProductShader::StaticForwardSheen);
    static_assert(SheenProgramCount == 2 * ClearcoatProgramCount);

    [[nodiscard]] constexpr bool IsSheenProgram(ProductShader role) noexcept
    {
        return role >= ProductShader::StaticForwardSheen && role < ProductShader::Count;
    }

    /// @brief 两个 sheen 颜色族共用受光顺序, 仅组合族同时消费清漆框架.
    [[nodiscard]] constexpr bool UsesClearcoatFrame(ProductShader role) noexcept
    {
        return IsClearcoatProgram(role) || (IsSheenProgram(role) && role >= ProductShader::StaticForwardSheenClearcoat);
    }

    /// @pre IsSheenProgram(role) 成立.
    [[nodiscard]] constexpr std::size_t SheenProgramIndex(ProductShader role) noexcept
    {
        return std::size_t(role) - std::size_t(ProductShader::StaticForwardSheen);
    }

    /// @brief 固定产品角色及获准的结构值. 零表示相应 define 缺席.
    struct VariantKey
    {
        ProductShader role = ProductShader::Count;
        std::uint32_t physicalLighting = 0;
        std::uint32_t iorSpecular = 0;
        std::uint32_t clearcoat = 0;
        std::uint32_t sheen = 0;
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
