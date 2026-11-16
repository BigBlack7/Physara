#include "SceneState.hpp"

/// @file
/// @brief 校验实例修改并叠加源材质, 保持模型和外部 CPU 图像的各自所有权.

#include <algorithm>
#include <cassert>
#include <cmath>

#include <Core/ResourceIO.hpp>

namespace Physara::Scene
{
    static constexpr std::array<Asset::ImageUse, MaterialSlotCount> SlotUses{
        Asset::ImageUse::BaseColor, Asset::ImageUse::OcclusionRoughnessMetallic, Asset::ImageUse::Normal,
        Asset::ImageUse::OcclusionRoughnessMetallic, Asset::ImageUse::Emissive
    };

    const Asset::ModelPrimitive *FindPrimitive(const Asset::ModelMesh &mesh, std::uint32_t slot) noexcept
    {
        // 导入为每个源 Primitive 保留一个有序槽, 包括无有效三角形的槽.
        return slot < mesh.primitives.size() ? &mesh.primitives[slot] : nullptr;
    }

    /// @brief 检查显式字段, 缺席值由已经验证的源材质提供.
    static bool ValidMaterial(const MaterialOverride &v) noexcept
    {
        const auto unit = [](float value) { return value >= 0 && value <= 1; };
        const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0; };
        return (!v.baseColor || std::ranges::all_of(*v.baseColor, unit)) &&
            (!v.emissive || std::ranges::all_of(*v.emissive, unit)) &&
            (!v.metallic || unit(*v.metallic)) && (!v.roughness || unit(*v.roughness)) &&
            (!v.normalScale || std::isfinite(*v.normalScale)) && (!v.occlusionStrength || unit(*v.occlusionStrength)) &&
            (!v.emissiveStrength || nonnegative(*v.emissiveStrength)) &&
            (!v.alphaCutoff || nonnegative(*v.alphaCutoff)) &&
            (!v.alphaMode || *v.alphaMode == Asset::ModelAlpha::Opaque || *v.alphaMode == Asset::ModelAlpha::Mask ||
             *v.alphaMode == Asset::ModelAlpha::Blend);
    }

    /// @brief 同时校验继承后采样所需的图像、用途和 UV, 以及可独立修改的参数.
    static bool ValidTexture(const TextureOverride &value, const std::optional<Asset::ModelTextureSlot> &base,
                             const Asset::ModelPrimitive &primitive, Asset::ImageUse use)
    {
        const auto wrap = [](auto field) { return !field || *field == 10497 || *field == 33071 || *field == 33648; };
        const auto filter = [](auto field) { return !field || *field == 9728 || *field == 9729; };
        const auto finite = [](const auto &field)
        {
            return !field || std::ranges::all_of(*field, [](float v) { return std::isfinite(v); });
        };
        const auto min = value.sampler.minFilter;
        const auto uv = value.texCoord.value_or(base ? base->texCoord : 0);
        if ((!base && !value.image) || static_cast<bool>(value.image) != value.use.has_value() ||
            (value.use && *value.use != use) ||
            uv >= primitive.hasUv.size() || !primitive.hasUv[uv] ||
            !wrap(value.sampler.wrapS) || !wrap(value.sampler.wrapT) || !filter(value.sampler.magFilter) ||
            (!filter(min) && (*min < 9984 || *min > 9987)) || !finite(value.offset) || !finite(value.scale) ||
            (value.rotationRadians && !std::isfinite(*value.rotationRadians)))
        {
            return false;
        }
        if (value.image)
        {
            constexpr std::array<std::wstring_view, 1> Directories{L"Textures"};
            return value.image->asset.IsValid() && value.image->revision.value != 0 &&
                std::holds_alternative<Core::ResourcePath>(Core::ResourcePath::Parse(value.image->source, Directories));
        }
        return true;
    }

    std::optional<Core::BoundedText> ValidateOverride(const Asset::ModelRecord &model,
                                                     const Asset::ModelPrimitive &primitive,
                                                     const PrimitiveOverride &value)
    {
        const auto &base = model.materials[primitive.material];
        if (!ValidMaterial(value.material))
        {
            return Core::BoundedText("Invalid material override field");
        }
        for (std::size_t i = 0; i < value.textures.size(); ++i)
        {
            if (value.textures[i] &&
                ((base.unlit && i != static_cast<std::size_t>(Asset::MaterialSlot::BaseColor)) ||
                 !ValidTexture(*value.textures[i], base.slots[i], primitive, SlotUses[i])))
            {
                return Core::BoundedText("Invalid texture override, usage, sampler or UV set");
            }
        }
        return std::nullopt;
    }

    /// @brief 对临时材质描述叠加已验证的实例字段. 源材质本身只读.
    static void ApplyMaterial(Asset::ModelMaterial &out, const MaterialOverride &value) noexcept
    {
        out.baseColor = value.baseColor.value_or(out.baseColor);
        out.emissive = value.emissive.value_or(out.emissive);
        out.metallic = value.metallic.value_or(out.metallic);
        out.roughness = value.roughness.value_or(out.roughness);
        out.normalScale = value.normalScale.value_or(out.normalScale);
        out.occlusionStrength = value.occlusionStrength.value_or(out.occlusionStrength);
        out.emissiveStrength = value.emissiveStrength.value_or(out.emissiveStrength);
        out.alphaMode = value.alphaMode.value_or(out.alphaMode);
        out.alphaCutoff = value.alphaCutoff.value_or(out.alphaCutoff);
        out.doubleSided = value.doubleSided.value_or(out.doubleSided);
    }

    /// @brief 解析源图像和 sampler, 可分别以实例值替换.
    static EffectiveTexture ResolveTexture(const Asset::ModelRecord &model,
                                           const std::optional<Asset::ModelTextureSlot> &source,
                                           const TextureOverride *over, Asset::ImageUse use) noexcept
    {
        EffectiveTexture out;
        if (source)
        {
            out.slot = *source;
            const auto &texture = model.textures[source->texture];
            out.sampler = model.samplers[texture.sampler];
            out.image = &model.images[texture.image];
        }
        if (over)
        {
            if (over->image)
            {
                auto image = Asset::DescribeImage(over->image, use);
                assert(image);
                out.image = std::move(image->image);
                out.slot.use = use;
                out.slot.domain = image->domain;
                out.slot.alpha = image->alpha;
            }
            out.slot.texCoord = over->texCoord.value_or(out.slot.texCoord);
            out.slot.offset = over->offset.value_or(out.slot.offset);
            out.slot.scale = over->scale.value_or(out.slot.scale);
            out.slot.rotationRadians = over->rotationRadians.value_or(out.slot.rotationRadians);
            if (over->sampler != SamplerOverride{})
            {
                out.sampler.sourceIndex.reset();
                if (over->sampler.minFilter)
                {
                    out.sampler.minFilter = over->sampler.minFilter;
                }
                if (over->sampler.magFilter)
                {
                    out.sampler.magFilter = over->sampler.magFilter;
                }
                out.sampler.wrapS = over->sampler.wrapS.value_or(out.sampler.wrapS);
                out.sampler.wrapT = over->sampler.wrapT.value_or(out.sampler.wrapT);
            }
        }
        return out;
    }

    EffectiveMaterial ResolveMaterial(const Asset::ModelRecord &model, const Asset::ModelPrimitive &primitive,
                                      const PrimitiveOverride *value) noexcept
    {
        EffectiveMaterial result{model.materials[primitive.material], {}, {}};
        const auto &base = result.values.slots;
        if (value)
        {
            ApplyMaterial(result.values, value->material);
        }
        constexpr auto Normal = static_cast<std::size_t>(Asset::MaterialSlot::Normal);
        if (primitive.hasTangent && base[Normal])
        {
            result.sourceTangentUv = base[Normal]->texCoord;
        }
        for (std::size_t i = 0; i < base.size(); ++i)
        {
            const auto *over = value && value->textures[i] ? &*value->textures[i] : nullptr;
            if (base[i] || over)
            {
                result.textures[i] = ResolveTexture(model, base[i], over, SlotUses[i]);
            }
        }
        return result;
    }
}
