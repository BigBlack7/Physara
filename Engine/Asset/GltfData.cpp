#include "GltfData.hpp"

/// @file
/// @brief 将 glTF 材质、图片用途、相机和灯转换为可独立持有的 CPU 记录.

#include <array>
#include <limits>
#include <numbers>
#include <utility>

namespace Physara::Asset::Gltf
{
    static constexpr double Pi = std::numbers::pi_v<double>;

    /// @brief 缺席使用 glTF 缺省值, 显式值独立校验并限制为可交付的 float32.
    static float Scalar(const Json &field, float fallback, float low, float high)
    {
        if (!field)
        {
            return fallback;
        }
        const auto value = field.Number();
        if (value < low || value > high)
        {
            field.Error("Value outside the supported range");
        }
        return static_cast<float>(value);
    }

    /// @brief 读取完整有限向量, 逐分量执行同一范围约束, 缺席时保留标准缺省.
    template<std::size_t N>
    static std::array<float, N> Vector(const Json &field, std::array<float, N> fallback,
                                      float low, float high)
    {
        if (field)
        {
            if (field.Size() != N)
            {
                field.Error("Unexpected array length");
            }
            for (std::size_t i = 0; i < N; ++i)
            {
                fallback[i] = Scalar(field.At(i), 0, low, high);
            }
        }
        return fallback;
    }

    /// @brief 将字段缺席保持为 optional, 显式非法值不能落入缺省分支.
    static std::optional<std::uint32_t> OptionalIndex(const Json &field)
    {
        return field ? std::optional(field.Index()) : std::nullopt;
    }

    /// @brief 保留 GLTF 指定的滤波和 wrap 枚举, 没有 filter 时不替下游决定.
    std::uint32_t Tables::Sampler(const Json &source)
    {
        const auto key = OptionalIndex(source);
        if (auto found = m_Samplers.find(key); found != m_Samplers.end())
        {
            return found->second;
        }
        ModelSampler sampler;
        sampler.sourceIndex = key;
        if (key)
        {
            const auto field = m_Root.Field("samplers").At(*key);
            sampler.magFilter = OptionalIndex(field.Field("magFilter"));
            sampler.minFilter = OptionalIndex(field.Field("minFilter"));
            auto wrap = [&field](std::string_view name)
            {
                const auto mode = field.Field(name);
                const auto value = mode ? mode.Index() : 10497;
                if (value != 10497 && value != 33071 && value != 33648)
                {
                    mode.Error("Invalid sampler wrap mode");
                }
                return value;
            };
            sampler.wrapS = wrap("wrapS");
            sampler.wrapT = wrap("wrapT");
            if (sampler.magFilter && *sampler.magFilter != 9728 && *sampler.magFilter != 9729)
            {
                field.Field("magFilter").Error("Invalid magnification filter");
            }
            if (sampler.minFilter && *sampler.minFilter != 9728 && *sampler.minFilter != 9729 &&
                (*sampler.minFilter < 9984 || *sampler.minFilter > 9987))
            {
                field.Field("minFilter").Error("Invalid minification filter");
            }
        }
        const auto index = static_cast<std::uint32_t>(m_Record.samplers.size());
        m_Record.samplers.push_back(sampler);
        m_Samplers.emplace(key, index);
        return index;
    }

    /// @brief 映射源 texture 和 sampler, 图像按源 image 下标共享一次输入而不合并采样用途.
    std::uint32_t Tables::Texture(const Json &source)
    {
        const auto key = source.Index();
        if (auto found = m_Textures.find(key); found != m_Textures.end())
        {
            return found->second;
        }
        const auto field = m_Root.Field("textures").At(key);
        const auto image = field.Field("source");
        if (!image)
        {
            field.Error("Texture has no supported core image fallback", AssetError::UnsupportedModel);
        }
        const auto sourceImage = image.Index();
        auto [mapping, inserted] = m_Images.emplace(sourceImage, static_cast<std::uint32_t>(m_ImageInputs.size()));
        if (inserted)
        {
            m_ImageInputs.emplace_back(sourceImage, m_Resources.Image(sourceImage));
        }
        const auto index = static_cast<std::uint32_t>(m_Record.textures.size());
        m_Record.textures.push_back({key, mapping->second, Sampler(field.Field("sampler"))});
        m_Textures.emplace(key, index);
        return index;
    }

    /// @brief 分离每槽的颜色域和 UV 变换, texCoord 覆盖在此确定, UV 可用性随后由 Primitive 校验.
    ModelTextureSlot Tables::Slot(const Json &field, ImageUse usage)
    {
        constexpr auto FloatMax = std::numeric_limits<float>::max();
        ModelTextureSlot slot;
        slot.texture = Texture(field.Field("index"));
        slot.texCoord = OptionalIndex(field.Field("texCoord")).value_or(0);
        slot.use = usage;
        // 此时图像尚未解码. 基色槽的覆盖语义在 DecodeImages 取得源通道数后确定.
        const auto sampling = *DescribeImageSampling(usage, false);
        slot.domain = sampling.domain;
        slot.alpha = sampling.alpha;
        const auto transform = field.Field("extensions").Field("KHR_texture_transform");
        slot.offset = Vector(transform.Field("offset"), slot.offset, -FloatMax, FloatMax);
        slot.scale = Vector(transform.Field("scale"), slot.scale, -FloatMax, FloatMax);
        slot.rotationRadians = Scalar(transform.Field("rotation"), 0, -FloatMax, FloatMax);
        slot.texCoord = OptionalIndex(transform.Field("texCoord")).value_or(slot.texCoord);
        return slot;
    }

    /// @brief 校验核心因子和已支持扩展, 只收集该材质实际使用的纹理, 不解释后续材质扩展.
    ModelMaterial Tables::ReadMaterial(const Json &field)
    {
        constexpr auto FloatMax = std::numeric_limits<float>::max();
        ModelMaterial material;
        const auto pbr = field.Field("pbrMetallicRoughness");
        const auto extensions = field.Field("extensions");
        const auto normal = field.Field("normalTexture");
        const auto occ = field.Field("occlusionTexture");
        const auto unlit = extensions.Field("KHR_materials_unlit");
        const auto ior = extensions.Field("KHR_materials_ior");
        const auto specular = extensions.Field("KHR_materials_specular");
        const auto clearcoat = extensions.Field("KHR_materials_clearcoat");
        const auto sheen = extensions.Field("KHR_materials_sheen");
        const auto coatNormal = clearcoat.Field("clearcoatNormalTexture");
        unlit.Object();
        ior.Object();
        specular.Object();
        clearcoat.Object();
        sheen.Object();
        const auto specularGlossiness = extensions.Field("KHR_materials_pbrSpecularGlossiness");
        // 在 Unlit 的基色专用分支之前判定同材质排斥. 空扩展对象也保留声明身份.
        if (clearcoat && (unlit || specularGlossiness))
        {
            clearcoat.Error("Clearcoat cannot be combined with KHR_materials_unlit or KHR_materials_pbrSpecularGlossiness");
        }
        material.hasSpecularGlossinessExtension = static_cast<bool>(specularGlossiness);
        material.hasClearcoatExtension = static_cast<bool>(clearcoat);
        material.clearcoatFactor = Scalar(clearcoat.Field("clearcoatFactor"), 0, 0, 1);
        material.clearcoatRoughnessFactor = Scalar(clearcoat.Field("clearcoatRoughnessFactor"), 0, 0, 1);
        material.clearcoatNormalScale = Scalar(coatNormal.Field("scale"), 1, -FloatMax, FloatMax);
        if (sheen && (unlit || specularGlossiness))
        {
            sheen.Error("Sheen cannot be combined with KHR_materials_unlit or KHR_materials_pbrSpecularGlossiness");
        }
        material.hasSheenExtension = static_cast<bool>(sheen);
        material.sheenColorFactor = Vector(sheen.Field("sheenColorFactor"), material.sheenColorFactor, 0, 1);
        material.sheenRoughnessFactor = Scalar(sheen.Field("sheenRoughnessFactor"), 0, 0, 1);
        if ((ior || specular) && (unlit || specularGlossiness))
        {
            (unlit ? unlit : specularGlossiness).Error(
                "Cannot be combined with KHR_materials_ior or KHR_materials_specular");
        }
        material.hasIorExtension = static_cast<bool>(ior);
        material.hasSpecularExtension = static_cast<bool>(specular);
        if (const auto input = ior.Field("ior"))
        {
            const auto number = input.Number();
            // 在窄化前区分合法的兼容零值和接近 0 或 1 的非法输入.
            if ((number != 0 && number < 1) || number > FloatMax)
            {
                input.Error("ior must be zero or a finite float32 value greater than or equal to 1");
            }
            material.ior = static_cast<float>(number);
        }
        material.specularFactor = Scalar(specular.Field("specularFactor"), 1, 0, 1);
        material.specularColorFactor =
            Vector(specular.Field("specularColorFactor"), material.specularColorFactor, 0, FloatMax);
        material.unlit = static_cast<bool>(unlit);
        material.baseColor = Vector(pbr.Field("baseColorFactor"), material.baseColor, 0, 1);
        material.emissive = Vector(field.Field("emissiveFactor"), material.emissive, 0, 1);
        material.metallic = Scalar(pbr.Field("metallicFactor"), 1, 0, 1);
        material.roughness = Scalar(pbr.Field("roughnessFactor"), 1, 0, 1);
        material.normalScale = Scalar(normal.Field("scale"), 1, -FloatMax, FloatMax);
        material.occlusionStrength = Scalar(occ.Field("strength"), 1, 0, 1);
        material.alphaCutoff = Scalar(field.Field("alphaCutoff"), 0.5f, 0, FloatMax);
        material.emissiveStrength = Scalar(
            extensions.Field("KHR_materials_emissive_strength").Field("emissiveStrength"), 1, 0, FloatMax);
        if (const auto doubleSided = field.Field("doubleSided"))
        {
            material.doubleSided = doubleSided.Boolean();
        }
        if (const auto alpha = field.Field("alphaMode"))
        {
            const auto value = alpha.Text();
            if (value == "BLEND")
            {
                material.alphaMode = ModelAlpha::Blend;
            }
            else if (value == "MASK")
            {
                material.alphaMode = ModelAlpha::Mask;
            }
            else if (value != "OPAQUE")
            {
                alpha.Error("Invalid material alphaMode");
            }
        }

        auto add = [this, &material](MaterialSlot kind, const Json &texture, ImageUse usage)
        {
            if (texture)
            {
                material.slots[static_cast<std::size_t>(kind)] = Slot(texture, usage);
            }
        };
        add(MaterialSlot::BaseColor, pbr.Field("baseColorTexture"), ImageUse::BaseColor);
        if (material.unlit)
        {
            // Unlit 仅消费基础颜色. 这些 PBR 纹理无需加入所选模型的依赖快照.
            return material;
        }
        add(MaterialSlot::MetallicRoughness, pbr.Field("metallicRoughnessTexture"), ImageUse::OcclusionRoughnessMetallic);
        add(MaterialSlot::Normal, normal, ImageUse::Normal);
        add(MaterialSlot::Occlusion, occ, ImageUse::OcclusionRoughnessMetallic);
        add(MaterialSlot::Emissive, field.Field("emissiveTexture"), ImageUse::Emissive);
        add(MaterialSlot::Specular, specular.Field("specularTexture"), ImageUse::Specular);
        add(MaterialSlot::SpecularColor, specular.Field("specularColorTexture"), ImageUse::SpecularColor);
        add(MaterialSlot::Clearcoat, clearcoat.Field("clearcoatTexture"), ImageUse::Clearcoat);
        add(MaterialSlot::ClearcoatRoughness, clearcoat.Field("clearcoatRoughnessTexture"), ImageUse::ClearcoatRoughness);
        add(MaterialSlot::ClearcoatNormal, coatNormal, ImageUse::ClearcoatNormal);
        add(MaterialSlot::SheenColor, sheen.Field("sheenColorTexture"), ImageUse::SheenColor);
        add(MaterialSlot::SheenRoughness, sheen.Field("sheenRoughnessTexture"), ImageUse::SheenRoughness);
        return material;
    }

    Tables::Tables(Json root, Resources &resources, ModelRecord &record)
        : m_Root(std::move(root)), m_Resources(resources), m_Record(record)
    {
    }

    std::uint32_t Tables::Material(const Json &source)
    {
        const auto key = OptionalIndex(source);
        if (auto found = m_Materials.find(key); found != m_Materials.end())
        {
            return found->second;
        }
        auto material = key ? ReadMaterial(m_Root.Field("materials").At(*key)) : ModelMaterial{};
        material.sourceIndex = key;
        const auto index = static_cast<std::uint32_t>(m_Record.materials.size());
        m_Record.materials.push_back(std::move(material));
        m_Materials.emplace(key, index);
        return index;
    }

    /// @brief 透视的缺席远面与 aspect 保持可选, 显式零不能充当无限远哨兵.
    static ModelPerspective ReadPerspective(const Json &field)
    {
        ModelPerspective out;
        out.yfov = field.Field("yfov").Number();
        out.znear = field.Field("znear").Number();
        if (out.yfov <= 0 || out.yfov >= Pi)
        {
            field.Field("yfov").Error("Expected 0 < yfov < pi", AssetError::UnsupportedModel);
        }
        if (out.znear <= 0)
        {
            field.Field("znear").Error("Perspective znear must be positive");
        }
        const auto far = field.Field("zfar");
        if (far)
        {
            out.zfar = far.Number();
            if (*out.zfar <= out.znear)
            {
                far.Error("Perspective zfar must exceed znear");
            }
        }
        const auto ratio = field.Field("aspectRatio");
        if (ratio)
        {
            out.aspectRatio = ratio.Number();
            if (*out.aspectRatio <= 0)
            {
                ratio.Error("Aspect ratio must be positive");
            }
        }
        return out;
    }

    /// @brief 保留正交投影原值, 将负 magnification 与项目 profile 不兼容单独报告.
    static ModelOrthographic ReadOrthographic(const Json &field)
    {
        ModelOrthographic out{field.Field("xmag").Number(), field.Field("ymag").Number(),
                              field.Field("znear").Number(), field.Field("zfar").Number()};
        if (out.xmag <= 0 || out.ymag <= 0)
        {
            field.Error("Expected positive orthographic magnifications", AssetError::UnsupportedModel);
        }
        if (out.znear < 0 || out.znear >= out.zfar)
        {
            field.Error("Expected 0 <= orthographic znear < zfar");
        }
        return out;
    }

    /// @brief 根据显式类型读取互斥的投影描述, 源编号保留给后续定位和实例化.
    static ModelCamera ReadCamera(const Json &field, std::uint32_t id)
    {
        const auto type = field.Field("type").Text();
        if (type == "perspective" && !field.Field("orthographic"))
        {
            return {id, ReadPerspective(field.Field("perspective"))};
        }
        if (type == "orthographic" && !field.Field("perspective"))
        {
            return {id, ReadOrthographic(field.Field("orthographic"))};
        }
        field.Error("Invalid camera type or conflicting projection fields");
    }

    /// @brief punctual light 保留源强度单位, 不预先折算渲染曝光.
    static ModelLight ReadLight(const Json &field, std::uint32_t id)
    {
        ModelLight out;
        out.sourceIndex = id;
        const auto kind = field.Field("type");
        const auto type = kind.Text();
        if (type == "directional")
        {
            out.kind = ModelLightKind::Directional;
            out.intensityUnit = ModelLightUnit::Lux;
        }
        else if (type == "point" || type == "spot")
        {
            out.kind = type == "point" ? ModelLightKind::Point : ModelLightKind::Spot;
            out.intensityUnit = ModelLightUnit::Candela;
        }
        else
        {
            kind.Error("Invalid punctual light type");
        }
        out.color = Vector(field.Field("color"), out.color, 0, 1);
        const auto intensity = field.Field("intensity");
        if (intensity)
        {
            out.intensity = intensity.Number();
            if (out.intensity < 0)
            {
                intensity.Error("Intensity must be nonnegative");
            }
        }
        const auto range = field.Field("range");
        if (range)
        {
            out.range = range.Number();
            if (out.kind == ModelLightKind::Directional || *out.range <= 0)
            {
                range.Error("Expected a positive point or spot range");
            }
        }
        const auto spot = field.Field("spot");
        if (out.kind == ModelLightKind::Spot)
        {
            if (!spot)
            {
                spot.Error("Spot light is missing spot descriptor");
            }
            const auto inner = spot.Field("innerConeAngle");
            const auto outer = spot.Field("outerConeAngle");
            out.innerConeAngle = inner ? inner.Number() : 0;
            out.outerConeAngle = outer ? outer.Number() : Pi / 4;
            if (out.innerConeAngle < 0 || out.outerConeAngle <= out.innerConeAngle || out.outerConeAngle > Pi / 2)
            {
                spot.Error("Expected 0 <= innerConeAngle < outerConeAngle <= pi/2");
            }
        }
        return out;
    }

    void Tables::PrepareNodes(std::stop_token stop)
    {
        const auto cameras = m_Root.Field("cameras");
        const auto lights = m_Root.Field("extensions").Field("KHR_lights_punctual").Field("lights");
        std::map<std::uint32_t, std::uint32_t> cameraMap;
        std::map<std::uint32_t, std::uint32_t> lightMap;
        for (auto &node : m_Record.nodes)
        {
            CheckStop(stop);
            if (node.camera)
            {
                const auto source = *node.camera;
                const auto [it, inserted] = cameraMap.emplace(source, static_cast<std::uint32_t>(m_Record.cameras.size()));
                if (inserted)
                {
                    m_Record.cameras.push_back(ReadCamera(cameras.At(source), source));
                }
                node.camera = it->second;
            }
            if (node.light)
            {
                const auto source = *node.light;
                const auto [it, inserted] = lightMap.emplace(source, static_cast<std::uint32_t>(m_Record.lights.size()));
                if (inserted)
                {
                    m_Record.lights.push_back(ReadLight(lights.At(source), source));
                }
                node.light = it->second;
            }
        }
    }

    void Tables::DecodeImages(const ImportLimits &limits, const std::shared_ptr<PixelBudget> &budget,
                              std::stop_token stop)
    {
        m_Record.images.reserve(m_ImageInputs.size());
        for (const auto &[index, input] : m_ImageInputs)
        {
            CheckStop(stop);
            const auto field = m_Root.Field("images").At(index);
            auto decoded = DecodeImage(input.bytes, limits.imageBytes, budget);
            CheckStop(stop);
            if (const auto *error = std::get_if<AssetFailure>(&decoded))
            {
                field.Error(error->description.View(), error->reason);
            }
            auto &pixels = std::get<Rgba8Image>(decoded);
            if (input.encoding && pixels.encoding != *input.encoding)
            {
                field.Error("Image MIME does not match decoded source");
            }
            m_Record.images.push_back({index, std::move(pixels)});
        }
        // 原图是否具有 alpha 只能在解码后确认, 但颜色/线性用途和材质 alphaMode 始终独立保存.
        for (auto &material : m_Record.materials)
        {
            auto &color = material.slots[static_cast<std::size_t>(MaterialSlot::BaseColor)];
            if (color)
            {
                const auto image = m_Record.textures[color->texture].image;
                color->alpha = DescribeImageSampling(color->use, m_Record.images[image].pixels.hasSourceAlpha)->alpha;
            }
        }
    }
}
