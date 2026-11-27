#pragma once

/// @file
/// @brief 静态实例材质的稀疏创作值和按需解析的只读描述.

#include <array>
#include <cstddef>
#include <optional>
#include <variant>

#include <Asset/ModelAsset.hpp>

namespace Physara::Scene
{
    /// @brief 缺席的字段沿用固定模型. 颜色因子在线性域.
    /// 显式的 IOR 或镜面字段参与 profile 选择, 即使数值与继承值相等也须保留到清除覆盖.
    struct MaterialOverride
    {
        std::optional<std::array<float, 4>> baseColor;
        std::optional<std::array<float, 3>> emissive;
        std::optional<float> metallic;
        std::optional<float> roughness;
        std::optional<float> normalScale;
        std::optional<float> occlusionStrength;
        std::optional<float> emissiveStrength;
        std::optional<Asset::ModelAlpha> alphaMode;
        std::optional<float> alphaCutoff;
        std::optional<bool> doubleSided;
        std::optional<float> ior;
        std::optional<float> specularFactor;
        std::optional<std::array<float, 3>> specularColorFactor;

        bool operator==(const MaterialOverride &) const = default;
    };

    /// @brief 标准 glTF 过滤和环绕枚举. 空值继承现有槽或使用标准缺省.
    struct SamplerOverride
    {
        std::optional<std::uint32_t> minFilter;
        std::optional<std::uint32_t> magFilter;
        std::optional<std::uint32_t> wrapS;
        std::optional<std::uint32_t> wrapT;

        bool operator==(const SamplerOverride &) const = default;
    };

    /// @brief 局部纹理修改. 有 image 时固定项目纹理版本, use 必须与槽的用途一致.
    /// 继承源图像的槽仍可单独覆盖采样器或 UV 变换. 各分量缺省时继续取源槽的有效值.
    struct TextureOverride
    {
        Asset::ImageVersion image;
        std::optional<Asset::ImageUse> use;
        std::optional<std::uint32_t> texCoord;
        SamplerOverride sampler;
        std::optional<std::array<float, 2>> offset;
        std::optional<std::array<float, 2>> scale;
        std::optional<float> rotationRadians;

        bool operator==(const TextureOverride &) const = default;
    };

    inline constexpr std::size_t MaterialSlotCount = static_cast<std::size_t>(Asset::MaterialSlot::Count);

    /// @brief 返回标准材质槽的图像用途, 由实例编辑、校验和采样解析共用.
    /// @pre slot 是有效的材质槽枚举.
    [[nodiscard]] Asset::ImageUse TextureUse(Asset::MaterialSlot slot) noexcept;

    /// @brief 槽位是 glTF 源 Primitive 下标, 由实体固定的模型版本解释.
    struct PrimitiveOverride
    {
        std::uint32_t primitiveSlot{};
        MaterialOverride material;
        std::array<std::optional<TextureOverride>, MaterialSlotCount> textures;

        bool operator==(const PrimitiveOverride &) const = default;
    };

    /// @brief 只读采样描述. ModelImage 借用模型版本, 其余参数为当前有效值.
    struct EffectiveTexture
    {
        std::variant<const Asset::ModelImage *, Asset::ImageVersion> image;
        Asset::ModelTextureSlot slot;
        Asset::ModelSampler sampler;
    };

    /// @brief 临时解析结果, values 的纹理索引仍描述源记录, 实际采样使用 textures.
    struct EffectiveMaterial
    {
        Asset::ModelMaterial values;
        std::array<std::optional<EffectiveTexture>, MaterialSlotCount> textures;
        std::optional<std::uint32_t> sourceTangentUv; ///< 原切线生成所用 UV, 可能与当前法线槽不同.
        bool iorSpecular{}; ///< 源声明或当前显式覆盖启用扩展模型.
    };

    /// @brief 从固定模型解析已验证的稀疏覆盖. value 为空时解析标准源材质.
    /// @pre primitive 属于 model, 非空 value 已按该 Primitive 校验. 本入口只解析, 不重复校验领域输入.
    /// @note 模型图像仍借用 model, 外置图像由返回值保有.
    [[nodiscard]] EffectiveMaterial ResolveMaterial(const Asset::ModelRecord &model,
                                                     const Asset::ModelPrimitive &primitive,
                                                     const PrimitiveOverride *value) noexcept;
}
