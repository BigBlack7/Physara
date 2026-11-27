#ifndef PHYSARA_CORE_SURFACE_GLSL
#define PHYSARA_CORE_SURFACE_GLSL

// 从有效 glTF 通道派生各向同性波瓣参数. 共享粗糙度过滤, 视角相关的 DFG 项由 BRDF 层准备.
#include <Source/Material/MaterialInputs.glsl>

// 色值为线性 RGB. 核心使用已混合的漫反射色和 F0, F90 固定为 1; 扩展保留未混合的基色.
// 过滤后感知粗糙度 p 用于 DFG 查表,
// alpha=p^2 用于 GGX/Smith. 后继消费者按各公式使用, 不把 alpha 再当作 p 或重复做几何过滤.
struct PixelParams
{
#ifdef PHYSARA_IOR_SPECULAR
    vec3 baseColor; // 介质漫反射的反照率及金属 F0.
    vec3 dielectricF0; // 权重应用前的介质 F0. F90 的权重前值为 1.
    float specularWeight;
    float metallic;
#else
    vec3 diffuseColor;
    vec3 f0;
    float f90;
#endif
    float perceptualRoughness;
    float alpha;
};

// roughness 是有效材质的原感知粗糙度, kernel 为 ComputeGeometricRoughnessKernel 的结果.
// 在 pRaw^4 域加入 kernel, 取两次平方根恢复 p, 限制到 [0.045, 1] 后再求 alpha=p^2.
float FilterCoreRoughness(float roughness, float kernel)
{
    float rawAlpha = roughness * roughness;
    float filteredAlphaSquared = clamp(rawAlpha * rawAlpha + kernel, 0.0, 1.0);
    // 0.045 是固定 Filament 的桌面感知粗糙度下限.
    return clamp(sqrt(sqrt(filteredAlphaSquared)), 0.045, 1.0);
}

#ifndef PHYSARA_IOR_SPECULAR
// 从已过滤的表面恢复核心波瓣, 屏幕解码与几何颜色入口共享此步.
PixelParams CorePixelParams(vec3 baseColor, float metallic, float filteredRoughness)
{
    return PixelParams(baseColor * (1.0 - metallic),
                       mix(vec3(0.04), baseColor, metallic),
                       1.0, filteredRoughness, filteredRoughness * filteredRoughness);
}
#endif

// 从已取样的通道构造波瓣. 扩展保留未加权的介质端点和独立强度, 供逐灯及 DFG 分别求值.
PixelParams BuildPixelParams(MaterialInputs inputs, float kernel)
{
    float roughness = FilterCoreRoughness(inputs.perceptualRoughness, kernel);
#ifdef PHYSARA_IOR_SPECULAR
    // KHR_materials_ior 的零值表示兼容模式. KHR_materials_specular 先限制有色 F0, 再在求值时乘权重.
    float reflectance = 1.0;
    if (inputs.ior != 0.0)
    {
        float amplitude = (inputs.ior - 1.0) / (inputs.ior + 1.0);
        reflectance = amplitude * amplitude;
    }
    return PixelParams(inputs.baseColor.rgb, min(vec3(1.0), reflectance * inputs.specularColor),
                       inputs.specularWeight, inputs.metallic, roughness, roughness * roughness);
#else
    return CorePixelParams(inputs.baseColor.rgb, inputs.metallic, roughness);
#endif
}

#endif
