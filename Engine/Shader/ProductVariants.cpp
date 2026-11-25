#include "ProductVariants.hpp"

/// @file
/// @brief 把产品程序名和结构字段解析为唯一的目录项, 为离线打包和重载提供相同的阶段入口.

#include <algorithm>
#include <charconv>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    // 角色值也是包数组下标. 三种受光配方各有带数组和不带数组的片元入口, 共享颜色实现.
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
        for (const auto &field : fields)
        {
            if (field.name != PhysicalLightingField || key.physicalLighting != 0 ||
                field.value != program->key.physicalLighting || field.value == 0)
            {
                Core::BoundedText reason("/program/defines: ");
                reason.Append(field.name);
                reason.Append("=");
                AppendValue(reason, field.value);
                if (field.name == PhysicalLightingField && key.physicalLighting != 0)
                {
                    reason.Append(" duplicates this field in the product variant.");
                }
                else
                {
                    reason.Append(" is not allowed for ");
                    reason.Append(program->name);
                    if (program->key.physicalLighting != 0)
                    {
                        reason.Append("; the approved combination requires ");
                        reason.Append(PhysicalLightingField);
                        reason.Append("=");
                        AppendValue(reason, program->key.physicalLighting);
                        reason.Append(".");
                    }
                    else
                    {
                        reason.Append("; this role declares no variant fields.");
                    }
                }
                return MakeFailure(ErrorCode::UnsupportedProfile, reason.View());
            }
            key.physicalLighting = field.value;
        }
        if (key != program->key)
        {
            return MakeFailure(ErrorCode::UnsupportedProfile,
                               "/program/defines: missing required PHYSARA_PHYSICAL_LIGHTING field.");
        }
        return key;
    }
}
