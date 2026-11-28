#include "SceneState.hpp"

/// @file
/// @brief 校验实例修改并叠加源材质, 保持模型和外部 CPU 图像的各自所有权.

#include <algorithm>
#include <cassert>
#include <cmath>

#include <Asset/StaticTangents.hpp>
#include <Core/ResourceIO.hpp>

namespace Physara::Scene
{
    /// @brief 各材质槽的采样解释与稳定诊断名称, 顺序对应 MaterialSlot.
    struct TextureSlotInfo
    {
        Asset::ImageUse use;
        std::string_view name;
    };

    static constexpr std::array<TextureSlotInfo, MaterialSlotCount> TextureSlots{{
        {Asset::ImageUse::BaseColor, "Base color"},
        {Asset::ImageUse::OcclusionRoughnessMetallic, "Metallic/roughness"},
        {Asset::ImageUse::Normal, "Normal"},
        {Asset::ImageUse::OcclusionRoughnessMetallic, "Occlusion"},
        {Asset::ImageUse::Emissive, "Emissive"},
        {Asset::ImageUse::Specular, "Specular strength"},
        {Asset::ImageUse::SpecularColor, "Specular color"},
        {Asset::ImageUse::Clearcoat, "Clearcoat strength"},
        {Asset::ImageUse::ClearcoatRoughness, "Clearcoat roughness"},
        {Asset::ImageUse::ClearcoatNormal, "Clearcoat normal"},
        {Asset::ImageUse::SheenColor, "Sheen color"},
        {Asset::ImageUse::SheenRoughness, "Sheen roughness"},
        {Asset::ImageUse::Anisotropy, "Anisotropy"}
    }};

    static constexpr auto Normal = static_cast<std::size_t>(Asset::MaterialSlot::Normal);
    static constexpr auto Coat = static_cast<std::size_t>(Asset::MaterialSlot::Clearcoat);
    static constexpr auto CoatRoughness = static_cast<std::size_t>(Asset::MaterialSlot::ClearcoatRoughness);
    static constexpr auto CoatNormal = static_cast<std::size_t>(Asset::MaterialSlot::ClearcoatNormal);
    static constexpr auto SheenColor = static_cast<std::size_t>(Asset::MaterialSlot::SheenColor);
    static constexpr auto SheenRoughness = static_cast<std::size_t>(Asset::MaterialSlot::SheenRoughness);
    static constexpr auto Anisotropy = static_cast<std::size_t>(Asset::MaterialSlot::Anisotropy);

    /// @brief sheen 是否需要颜色角色只由采样前的有效因子决定.
    static bool HasSheenColor(const std::array<float, 3> &color) noexcept
    {
        return std::ranges::any_of(color, [](float component) { return component > 0; });
    }

    /// @brief 采用扩展字段或槽的存在性, 保留显式零值的覆盖身份.
    static bool HasClearcoatOverride(const PrimitiveOverride &value) noexcept
    {
        return value.material.clearcoatFactor || value.material.clearcoatRoughnessFactor ||
            value.material.clearcoatNormalScale || value.textures[Coat] || value.textures[CoatRoughness] ||
            value.textures[CoatNormal];
    }

    /// @brief 稀疏方向输入也有身份, 显式零强度不会清除其它字段.
    static bool HasAnisotropyOverride(const PrimitiveOverride &value) noexcept
    {
        return value.material.anisotropyStrength || value.material.anisotropyRotation || value.textures[Anisotropy];
    }

    /// @brief 源切线属于基底法线槽的 UV, 只有独立清漆槽时取该槽的 UV.
    static std::optional<std::uint32_t> SourceTangentUv(const Asset::ModelMaterial &base,
                                                        const Asset::ModelPrimitive &primitive) noexcept
    {
        const auto &slot = base.slots[Normal] ? base.slots[Normal] : base.slots[CoatNormal];
        return primitive.hasTangent && slot ? std::optional(slot->texCoord) : std::nullopt;
    }

    Asset::ImageUse TextureUse(Asset::MaterialSlot slot) noexcept
    {
        const auto index = static_cast<std::size_t>(slot);
        assert(index < TextureSlots.size());
        return TextureSlots[index].use;
    }

    const Asset::ModelPrimitive *FindPrimitive(const Asset::ModelMesh &mesh, std::uint32_t slot) noexcept
    {
        // 导入为每个源 Primitive 保留一个有序槽, 包括无有效三角形的槽.
        return slot < mesh.primitives.size() ? &mesh.primitives[slot] : nullptr;
    }

    /// @brief 检查显式字段并返回它的值域约束. 缺席值由已经验证的源材质提供.
    static const char *MaterialError(const MaterialOverride &v, const Asset::ModelMaterial &source) noexcept
    {
        const auto unit = [](float value) { return value >= 0 && value <= 1; };
        const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0; };
        if (v.baseColor && !std::ranges::all_of(*v.baseColor, unit))
        {
            return "Linear base color RGBA must have finite components in [0, 1]";
        }
        if (v.emissive && !std::ranges::all_of(*v.emissive, unit))
        {
            return "Linear emissive RGB must have finite components in [0, 1]";
        }
        if (v.metallic && !unit(*v.metallic))
        {
            return "Metallic must be finite and in [0, 1]";
        }
        if (v.roughness && !unit(*v.roughness))
        {
            return "Roughness must be finite and in [0, 1]";
        }
        if (v.normalScale && !std::isfinite(*v.normalScale))
        {
            return "Normal scale must be finite";
        }
        if (v.occlusionStrength && !unit(*v.occlusionStrength))
        {
            return "Occlusion strength must be finite and in [0, 1]";
        }
        if (v.emissiveStrength && !nonnegative(*v.emissiveStrength))
        {
            return "Emissive strength must be finite and nonnegative";
        }
        if (v.alphaCutoff && !nonnegative(*v.alphaCutoff))
        {
            return "Alpha cutoff must be finite and nonnegative";
        }
        if (source.unlit && (v.ior || v.specularFactor || v.specularColorFactor))
        {
            return "IOR and specular overrides require a lit material";
        }
        if (v.ior)
        {
            if (source.ior == 0)
            {
                return "IOR is read-only for a source in compatibility mode";
            }
            if (!std::isfinite(*v.ior) || *v.ior < 1)
            {
                return "IOR override must be finite and greater than or equal to 1";
            }
        }
        if (v.specularFactor && !unit(*v.specularFactor))
        {
            return "Specular factor must be finite and in [0, 1]";
        }
        if (v.specularColorFactor && !std::ranges::all_of(*v.specularColorFactor, nonnegative))
        {
            return "Linear specular color must have finite, nonnegative RGB components";
        }
        if (v.clearcoatFactor && !unit(*v.clearcoatFactor))
        {
            return "Clearcoat factor must be finite and in [0, 1]";
        }
        if (v.clearcoatRoughnessFactor && !unit(*v.clearcoatRoughnessFactor))
        {
            return "Clearcoat roughness factor must be finite and in [0, 1]";
        }
        if (v.clearcoatNormalScale && !std::isfinite(*v.clearcoatNormalScale))
        {
            return "Clearcoat normal scale must be finite";
        }
        if (v.sheenColorFactor && !std::ranges::all_of(*v.sheenColorFactor, unit))
        {
            return "Linear sheen color must have finite RGB components in [0, 1]";
        }
        if (v.sheenRoughnessFactor && !unit(*v.sheenRoughnessFactor))
        {
            return "Sheen roughness factor must be finite and in [0, 1]";
        }
        if (v.anisotropyStrength && !unit(*v.anisotropyStrength))
        {
            return "Anisotropy strength must be finite and in [0, 1]";
        }
        if (v.anisotropyRotation && !std::isfinite(*v.anisotropyRotation))
        {
            return "Anisotropy rotation must be finite radians";
        }
        using Alpha = Asset::ModelAlpha;
        if (v.alphaMode && *v.alphaMode != Alpha::Opaque && *v.alphaMode != Alpha::Mask && *v.alphaMode != Alpha::Blend)
        {
            return "Alpha mode must be OPAQUE, MASK or BLEND";
        }
        return nullptr;
    }

    /// @brief 同时校验继承后采样所需的图像、用途和 UV, 以及可独立修改的参数.
    static const char *TextureError(const TextureOverride &value, const std::optional<Asset::ModelTextureSlot> &base,
                                    const Asset::ModelPrimitive &primitive, Asset::ImageUse use)
    {
        const auto wrap = [](auto field) { return !field || *field == 10497 || *field == 33071 || *field == 33648; };
        const auto filter = [](auto field) { return !field || *field == 9728 || *field == 9729; };
        const auto finite = [](const auto &field)
        {
            return !field || std::ranges::all_of(*field, [](float v) { return std::isfinite(v); });
        };
        const auto min = value.sampler.minFilter;
        // 替换图像可继续使用原槽的 UV. 新槽才从 glTF 缺省的 UV0 起算.
        const auto uv = value.texCoord.value_or(base ? base->texCoord : 0);
        if ((!base && !value.image) || static_cast<bool>(value.image) != value.use.has_value() ||
            (value.use && *value.use != use))
        {
            return "Texture slot requires a compatible source image and usage";
        }
        if (uv >= primitive.hasUv.size() || !primitive.hasUv[uv])
        {
            return "Texture UV set is missing from this Primitive";
        }
        if (!wrap(value.sampler.wrapS) || !wrap(value.sampler.wrapT) || !filter(value.sampler.magFilter) ||
            (!filter(min) && (*min < 9984 || *min > 9987)))
        {
            return "Texture sampler must use standard glTF filters and wrap modes";
        }
        if (!finite(value.offset) || !finite(value.scale) ||
            (value.rotationRadians && !std::isfinite(*value.rotationRadians)))
        {
            return "Texture UV transform must be finite";
        }
        if (value.image)
        {
            constexpr std::array<std::wstring_view, 1> Directories{L"Textures"};
            if (!value.image->asset.IsValid() || value.image->revision.value == 0 ||
                !std::holds_alternative<Core::ResourcePath>(
                    Core::ResourcePath::Parse(value.image->source, Directories)))
            {
                return "Texture override requires a published Assets/Textures image";
            }
        }
        return nullptr;
    }

    /// @brief 取得法线槽的有效 UV. 新建槽沿用 glTF 的 UV0 缺省, 空槽没有坐标需求.
    static std::optional<std::uint32_t> NormalUv(const Asset::ModelMaterial &base,
                                                const PrimitiveOverride *value, std::size_t slot) noexcept
    {
        const auto source = base.slots[slot] ? std::optional(base.slots[slot]->texCoord) : std::nullopt;
        if (value && value->textures[slot])
        {
            return value->textures[slot]->texCoord.value_or(source.value_or(0));
        }
        return source;
    }

    /// @brief 继承已确认的同 UV 框架. 未确认的有效 UV 通过角点生成校验, 失败定位到实际槽.
    static std::optional<Core::BoundedText> NormalFrameError(const Asset::ModelPrimitive &primitive,
        const Asset::ModelMaterial &base, std::size_t slot, std::optional<std::uint32_t> uv,
        const PrimitiveOverride *previous)
    {
        if (!uv || (previous && uv == NormalUv(base, previous, slot)) || uv == NormalUv(base, nullptr, slot) ||
            uv == SourceTangentUv(base, primitive))
        {
            return std::nullopt;
        }
        const auto location = slot == CoatNormal ? "clearcoatNormalTexture.texCoord" : "normalTexture.texCoord";
        if (const auto error = Asset::ValidateNormalUvFrame(primitive, *uv, location))
        {
            return error->description;
        }
        return std::nullopt;
    }

    std::optional<Core::BoundedText> ValidateOverride(const Asset::ModelRecord &model,
                                                     const Asset::ModelPrimitive &primitive,
                                                     const PrimitiveOverride &value,
                                                     const PrimitiveOverride *previous)
    {
        const auto &base = model.materials[primitive.material];
        if (const auto *error = MaterialError(value.material, base))
        {
            return Core::BoundedText(error);
        }
        const bool coatOverride = HasClearcoatOverride(value);
        if (coatOverride && (base.unlit || base.hasSpecularGlossinessExtension))
        {
            return Core::BoundedText("Clearcoat overrides require a compatible lit metallic-roughness material");
        }
        const bool sheenOverride = value.material.sheenColorFactor || value.material.sheenRoughnessFactor ||
                                   value.textures[SheenColor] || value.textures[SheenRoughness];
        if (sheenOverride && (base.unlit || base.hasSpecularGlossinessExtension))
        {
            return Core::BoundedText("Sheen overrides require a compatible lit metallic-roughness material");
        }
        const bool anisotropyOverride = HasAnisotropyOverride(value);
        if (anisotropyOverride && (base.unlit || base.hasSpecularGlossinessExtension))
        {
            return Core::BoundedText("Anisotropy overrides require a compatible lit metallic-roughness material");
        }
        for (std::size_t i = 0; i < value.textures.size(); ++i)
        {
            if (!value.textures[i])
            {
                continue;
            }
            const auto &slot = TextureSlots[i];
            const auto *error = base.unlit && i != static_cast<std::size_t>(Asset::MaterialSlot::BaseColor) ?
                "Unlit supports only a base color texture" :
                TextureError(*value.textures[i], base.slots[i], primitive, slot.use);
            if (error)
            {
                Core::BoundedText detail(slot.name);
                detail.Append(" texture: ");
                detail.Append(error);
                return detail;
            }
        }
        const bool clearcoat = base.hasClearcoatExtension || coatOverride;
        const bool anisotropy = base.hasAnisotropyExtension || anisotropyOverride;
        const auto strength = value.material.anisotropyStrength.value_or(base.anisotropyStrength);
        if (strength > 0 && (clearcoat || HasSheenColor(value.material.sheenColorFactor.value_or(base.sheenColorFactor))))
        {
            return Core::BoundedText("Anisotropy combined with clearcoat or active sheen is unsupported");
        }
        if (clearcoat || anisotropy)
        {
            const auto baseUv = NormalUv(base, &value, Normal);
            const auto coatUv = clearcoat ? NormalUv(base, &value, CoatNormal) : std::nullopt;
            if (!baseUv && !primitive.hasSourceTangent && (anisotropy || coatUv))
            {
                return Core::BoundedText(anisotropy ? "Anisotropy requires source tangents or a usable base normal frame" :
                    "Clearcoat normal texture requires source tangents or a usable base normal frame");
            }
            // 核心覆盖可能只校验过 UV 存在性. 曾由扩展确认的基础框架, 可由同一 UV 继续使用.
            const auto *confirmed = previous && (base.hasClearcoatExtension || HasClearcoatOverride(*previous) ||
                base.hasAnisotropyExtension || HasAnisotropyOverride(*previous)) ? previous : nullptr;
            if (auto error = NormalFrameError(primitive, base, Normal, baseUv, confirmed))
            {
                return error;
            }
            // 同一 UV 的两槽共用未变换切线. 各自的采样变换在着色时应用, 此处只需校验一次角点框架.
            if (coatUv && coatUv != baseUv)
            {
                return NormalFrameError(primitive, base, CoatNormal, coatUv, confirmed);
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
        out.ior = value.ior.value_or(out.ior);
        out.specularFactor = value.specularFactor.value_or(out.specularFactor);
        out.specularColorFactor = value.specularColorFactor.value_or(out.specularColorFactor);
        out.clearcoatFactor = value.clearcoatFactor.value_or(out.clearcoatFactor);
        out.clearcoatRoughnessFactor = value.clearcoatRoughnessFactor.value_or(out.clearcoatRoughnessFactor);
        out.clearcoatNormalScale = value.clearcoatNormalScale.value_or(out.clearcoatNormalScale);
        out.sheenColorFactor = value.sheenColorFactor.value_or(out.sheenColorFactor);
        out.sheenRoughnessFactor = value.sheenRoughnessFactor.value_or(out.sheenRoughnessFactor);
        out.anisotropyStrength = value.anisotropyStrength.value_or(out.anisotropyStrength);
        out.anisotropyRotation = value.anisotropyRotation.value_or(out.anisotropyRotation);
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
        result.iorSpecular = result.values.hasIorExtension || result.values.hasSpecularExtension;
        result.clearcoat = result.values.hasClearcoatExtension || (value && HasClearcoatOverride(*value));
        if (value)
        {
            ApplyMaterial(result.values, value->material);
            constexpr auto Specular = static_cast<std::size_t>(Asset::MaterialSlot::Specular);
            constexpr auto SpecularColor = static_cast<std::size_t>(Asset::MaterialSlot::SpecularColor);
            result.iorSpecular = result.iorSpecular || value->material.ior || value->material.specularFactor ||
                                 value->material.specularColorFactor || value->textures[Specular] ||
                                 value->textures[SpecularColor];
        }
        // 显式零值先覆盖源因子, Draw 再从有效颜色和强度选角色. 方向图 B 只影响单个片元.
        result.sheen = HasSheenColor(result.values.sheenColorFactor);
        result.anisotropy = result.values.anisotropyStrength > 0;
        result.sourceTangentUv = SourceTangentUv(result.values, primitive);
        for (std::size_t i = 0; i < base.size(); ++i)
        {
            const auto *over = value && value->textures[i] ? &*value->textures[i] : nullptr;
            if (base[i] || over)
            {
                result.textures[i] = ResolveTexture(model, base[i], over, TextureSlots[i].use);
            }
        }
        return result;
    }
}
