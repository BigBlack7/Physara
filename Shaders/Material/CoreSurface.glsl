#ifndef PHYSARA_CORE_SURFACE_GLSL
#define PHYSARA_CORE_SURFACE_GLSL

// 从有效 glTF 通道一次性派生核心各向同性波瓣参数, 不读取绑定或执行纹理取样.
// 颜色混合及几何粗糙度过滤供各着色消费者共用, 视角相关的 DFG 项由 BRDF 层另行准备.
#include <Source/Material/MaterialInputs.glsl>

// diffuseColor 和 f0 为线性 RGB, 核心 f90 固定为 1. 过滤后感知粗糙度 p 用于 DFG 查表,
// alpha=p^2 用于 GGX/Smith. 后继消费者按各公式使用, 不把 alpha 再当作 p 或重复做几何过滤.
struct PixelParams
{
    vec3 diffuseColor;
    vec3 f0;
    float f90;
    float perceptualRoughness;
    float alpha;
};

// inputs 遵守 MaterialInputs 的有效范围; kernel 为 ComputeGeometricRoughnessKernel 的结果.
// 在 pRaw^4 域加入 kernel, 取两次平方根恢复 p, 限制到 [0.045, 1] 后再求 alpha=p^2.
// 基色及金属度派生 diffuseColor/F0; alpha 覆盖与 AO 不参与波瓣参数, 由各自消费者处理.
PixelParams BuildPixelParams(MaterialInputs inputs, float kernel)
{
    float roughness = inputs.perceptualRoughness;
    float rawAlpha = roughness * roughness;
    float filteredAlphaSquared = clamp(rawAlpha * rawAlpha + kernel, 0.0, 1.0);
    // 0.045 是固定 Filament 的桌面感知粗糙度下限.
    float filteredRoughness = clamp(sqrt(sqrt(filteredAlphaSquared)), 0.045, 1.0);
    vec3 baseColor = inputs.baseColor.rgb;
    return PixelParams(baseColor * (1.0 - inputs.metallic),
                       mix(vec3(0.04), baseColor, inputs.metallic),
                       1.0, filteredRoughness, filteredRoughness * filteredRoughness);
}

#endif
