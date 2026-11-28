#include "ProductVariants.hpp"

/// @file
/// @brief 把产品程序名和结构字段解析为唯一的目录项, 为离线打包和重载提供相同的阶段入口.

#include <algorithm>
#include <charconv>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    // 角色值也是包数组下标. Direct, DFG 和 IBL 按候选来源及是否读取阴影数组选择片元入口.
    // 三种诊断共用数组查询, 以入口决定配色. Hard/PCF 和级联数量均随本帧数据传入.
    static constexpr std::array<ProductProgram, ProductShaderCount> Programs{{
        {{ProductShader::StaticForward}, "StaticForward", "StaticForward",
         "Passes/Forward/StaticForward.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticDepth}, "StaticDepth", "StaticForward",
         "Passes/Forward/StaticDepth.json", "Passes/Forward/Static.vert", "Passes/Forward/Depth.frag"},
        {{ProductShader::ForwardDisplay}, "ForwardDisplay", "ForwardDisplay",
         "Passes/Forward/ForwardDisplay.json", "Passes/Forward/Display.vert", "Passes/Forward/Display.frag"},
        {{ProductShader::StaticForwardPhysical, 1}, "StaticForwardPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPhysical.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIbl, 2}, "StaticForwardIbl", "StaticForward",
         "Passes/Forward/StaticForwardIbl.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::ForwardSky}, "ForwardSky", "ForwardSky",
         "Passes/Forward/ForwardSky.json", "Passes/Forward/Sky.vert", "Passes/Forward/Sky.frag"},
        {{ProductShader::DisplayColorGrading}, "DisplayColorGrading", "DisplayPost",
         "PostProcess/DisplayColorGrading.json", "Passes/Forward/Display.vert", "PostProcess/DisplayColorGrading.frag"},
        {{ProductShader::DisplayFxaa}, "DisplayFxaa", "DisplayPost",
         "PostProcess/DisplayFxaa.json", "Passes/Forward/Display.vert", "PostProcess/DisplayFxaa.frag"},
        {{ProductShader::DisplayScale}, "DisplayScale", "DisplayPost",
         "PostProcess/DisplayScale.json", "Passes/Forward/Display.vert", "PostProcess/DisplayScale.frag"},
        {{ProductShader::StaticShadowDepth}, "StaticShadowDepth", "StaticForward",
         "Passes/Shadow/StaticShadowDepth.json", "Passes/Shadow/Depth.vert", "Passes/Forward/Depth.frag"},
        {{ProductShader::StaticCascadePreview}, "StaticCascadePreview", "StaticForward",
         "Passes/Shadow/StaticCascadePreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/CascadePreview.frag"},
        {{ProductShader::StaticSpotShadowPreview}, "StaticSpotShadowPreview", "StaticForward",
         "Passes/Shadow/StaticSpotShadowPreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/SpotPreview.frag"},
        {{ProductShader::StaticPointShadowPreview}, "StaticPointShadowPreview", "StaticForward",
         "Passes/Shadow/StaticPointShadowPreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/PointPreview.frag"},
        {{ProductShader::StaticForwardShadows}, "StaticForwardShadows", "StaticForward",
         "Passes/Forward/StaticForwardShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPhysicalShadows, 1}, "StaticForwardPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPhysicalShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIblShadows, 2}, "StaticForwardIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardIblShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlus}, "StaticForwardPlus", "StaticForward",
         "Passes/Forward/StaticForwardPlus.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusPhysical, 1}, "StaticForwardPlusPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusPhysical.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIbl, 2}, "StaticForwardPlusIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusIbl.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusShadows}, "StaticForwardPlusShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusPhysicalShadows, 1}, "StaticForwardPlusPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIblShadows, 2}, "StaticForwardPlusIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticCoreGBuffer}, "StaticCoreGBuffer", "StaticForward",
         "Passes/GBuffer/StaticCoreGBuffer.json", "Passes/Forward/Static.vert", "Passes/GBuffer/Core.frag"},
        {{ProductShader::StaticSurfaceAux}, "StaticSurfaceAux", "StaticForward",
         "Passes/GBuffer/StaticSurfaceAux.json", "Passes/Forward/Static.vert", "Passes/GBuffer/SurfaceAux.frag"},
        {{ProductShader::DeferredLighting}, "DeferredLighting", "StaticForward",
         "Passes/Deferred/DeferredLighting.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingPhysical, 1}, "DeferredLightingPhysical", "StaticForward",
         "Passes/Deferred/DeferredLightingPhysical.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingIbl, 2}, "DeferredLightingIbl", "StaticForward",
         "Passes/Deferred/DeferredLightingIbl.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingShadows}, "DeferredLightingShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::DeferredLightingPhysicalShadows, 1}, "DeferredLightingPhysicalShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingPhysicalShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::DeferredLightingIblShadows, 2}, "DeferredLightingIblShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingIblShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::SurfaceDepthPreview}, "SurfaceDepthPreview", "SurfacePreview",
         "Passes/SurfacePreview/SurfaceDepthPreview.json", "Passes/Forward/Display.vert",
         "Passes/SurfacePreview/Depth.frag"},
        {{ProductShader::SurfaceGBufferPreview}, "SurfaceGBufferPreview", "SurfacePreview",
         "Passes/SurfacePreview/SurfaceGBufferPreview.json", "Passes/Forward/Display.vert",
         "Passes/SurfacePreview/GBuffer.frag"},
        {{ProductShader::StaticForwardIorSpecular, 0, 1}, "StaticForwardIorSpecular", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecular.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularPhysical, 1, 1}, "StaticForwardIorSpecularPhysical", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularIbl, 2, 1}, "StaticForwardIorSpecularIbl", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularShadows, 0, 1}, "StaticForwardIorSpecularShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIorSpecularPhysicalShadows, 1, 1}, "StaticForwardIorSpecularPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIorSpecularIblShadows, 2, 1}, "StaticForwardIorSpecularIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecular, 0, 1}, "StaticForwardPlusIorSpecular", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecular.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularPhysical, 1, 1}, "StaticForwardPlusIorSpecularPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularIbl, 2, 1}, "StaticForwardPlusIorSpecularIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularShadows, 0, 1}, "StaticForwardPlusIorSpecularShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularPhysicalShadows, 1, 1}, "StaticForwardPlusIorSpecularPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularIblShadows, 2, 1}, "StaticForwardPlusIorSpecularIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoat, 0, 0, 1}, "StaticForwardClearcoat", "StaticForward",
         "Passes/Forward/StaticForwardClearcoat.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatPhysical, 1, 0, 1}, "StaticForwardClearcoatPhysical", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatIbl, 2, 0, 1}, "StaticForwardClearcoatIbl", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatShadows, 0, 0, 1}, "StaticForwardClearcoatShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoatPhysicalShadows, 1, 0, 1}, "StaticForwardClearcoatPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoatIblShadows, 2, 0, 1}, "StaticForwardClearcoatIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoat, 0, 0, 1}, "StaticForwardPlusClearcoat", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoat.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatPhysical, 1, 0, 1}, "StaticForwardPlusClearcoatPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatIbl, 2, 0, 1}, "StaticForwardPlusClearcoatIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatShadows, 0, 0, 1}, "StaticForwardPlusClearcoatShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatPhysicalShadows, 1, 0, 1}, "StaticForwardPlusClearcoatPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatIblShadows, 2, 0, 1}, "StaticForwardPlusClearcoatIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
    }};

    // 名称查找和按枚举索引须落在同一项, 重名或错位在构建目录时就会报错.
    static_assert([]
    {
        for (std::size_t index = 0; index < Programs.size(); ++index)
        {
            if (static_cast<std::size_t>(Programs[index].key.role) != index ||
                std::ranges::count(Programs, Programs[index].name, &ProductProgram::name) != 1)
            {
                return false;
            }
        }
        return true;
    }());

    std::span<const ProductProgram, ProductShaderCount> ProductPrograms() noexcept
    {
        return Programs;
    }

    const ProductProgram *FindProductProgram(ProductShader role) noexcept
    {
        const auto index = static_cast<std::size_t>(role);
        return index < Programs.size() ? &Programs[index] : nullptr;
    }

    const ProductProgram *FindProductProgram(std::string_view name) noexcept
    {
        const auto found = std::ranges::find(Programs, name, &ProductProgram::name);
        return found == Programs.end() ? nullptr : &*found;
    }

    // 结构值为 uint32_t, 固定缓冲可容纳全部十进制位. 错误路径无需分配.
    static void AppendValue(Core::BoundedText &description, std::uint32_t value) noexcept
    {
        std::array<char, 10> digits;
        const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
        description.Append({digits.data(), static_cast<std::size_t>(result.ptr - digits.data())});
    }

    Result<VariantKey> ResolveVariant(std::string_view name, std::span<const VariantField> fields) noexcept
    {
        const auto *program = FindProductProgram(name);
        if (!program)
        {
            return MakeFailure(ErrorCode::UnsupportedProfile, "Unknown product Shader role.");
        }
        VariantKey key{program->key.role};
        constexpr std::array members{
            std::pair{PhysicalLightingField, &VariantKey::physicalLighting},
            std::pair{IorSpecularField, &VariantKey::iorSpecular},
            std::pair{ClearcoatField, &VariantKey::clearcoat}};
        for (const auto &field : fields)
        {
            const auto found = std::ranges::find_if(members, [&](const auto &entry) { return entry.first == field.name; });
            const auto member = found == members.end() ? nullptr : found->second;
            if (!member || key.*member != 0 || field.value != program->key.*member || field.value == 0)
            {
                Core::BoundedText reason("/program/defines: ");
                reason.Append(field.name);
                reason.Append("=");
                AppendValue(reason, field.value);
                if (member && key.*member != 0)
                {
                    reason.Append(" duplicates this field in the product variant.");
                }
                else
                {
                    reason.Append(" is not allowed for ");
                    reason.Append(program->name);
                    if (member && program->key.*member != 0)
                    {
                        reason.Append(". The approved combination requires ");
                        reason.Append(found->first);
                        reason.Append("=");
                        AppendValue(reason, program->key.*member);
                        reason.Append(".");
                    }
                    else
                    {
                        reason.Append(". This field is outside the role's declared variant.");
                    }
                }
                return MakeFailure(ErrorCode::UnsupportedProfile, reason.View());
            }
            key.*member = field.value;
        }
        for (const auto &[fieldName, member] : members)
        {
            if (key.*member != program->key.*member)
            {
                Core::BoundedText reason("/program/defines: missing required ");
                reason.Append(fieldName);
                reason.Append(" field.");
                return MakeFailure(ErrorCode::UnsupportedProfile, reason.View());
            }
        }
        return key;
    }
}
