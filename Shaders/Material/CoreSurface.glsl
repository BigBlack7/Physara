#ifndef PHYSARA_CORE_SURFACE_GLSL
#define PHYSARA_CORE_SURFACE_GLSL

// 从有效 glTF 通道派生各向同性波瓣参数. 共享粗糙度过滤, 视角相关的 DFG 项由 BRDF 层准备.
#include <Source/Material/MaterialInputs.glsl>

// 色值为线性 RGB. 核心使用已混合的漫反射色和 F0, F90 固定为 1; 扩展保留未混合的基色.
// 过滤后感知粗糙度 p 用于 DFG 查表,
// alpha=p^2 用于 GGX/Smith. 后继消费者按各公式使用, 不把 alpha 再当作 p 或重复做几何过滤.
struct PixelParams
{
    vec3 diffuseColor;
    vec3 f0;
    float f90;
    vec3 baseColor; // 介质漫反射的反照率及金属 F0.
    vec3 dielectricF0; // 权重应用前的介质 F0. F90 的权重前值为 1.
    float specularWeight;
    float metallic;
    float perceptualRoughness;
    float alpha;
#if defined(PHYSARA_CLEARCOAT) || defined(PHYSARA_SHEEN)
    bool iorSpecularBase;
#endif
#ifdef PHYSARA_CLEARCOAT
    float coatWeight; // 先持有层强度, 光照前乘一次视向 Fresnel.
    float coatRoughness;
    float coatAlpha;
#endif
#ifdef PHYSARA_SHEEN
    vec3 sheenColor;
    float sheenRoughness;
    float sheenAlpha;
    float sheenScale; // 零表示当前片元无 sheen 波瓣, 否则为逐灯方向归一乘数.
    float sheenAlbedo; // 限制到 [0, 1] 的匹配 Charlie/Neubelt 方向反照率.
#endif
};

const float MinPerceptualRoughness = 0.045; // 固定 Filament 的桌面感知粗糙度下限.

// roughness 是有效材质的原感知粗糙度, kernel 为 ComputeGeometricRoughnessKernel 的结果.
// 在 pRaw^4 域加入 kernel, 取两次平方根恢复 p, 限制到 [0.045, 1] 后再求 alpha=p^2.
float FilterCoreRoughness(float roughness, float kernel)
{
    float rawAlpha = roughness * roughness;
    float filteredAlphaSquared = clamp(rawAlpha * rawAlpha + kernel, 0.0, 1.0);
    return clamp(sqrt(sqrt(filteredAlphaSquared)), MinPerceptualRoughness, 1.0);
}

// 叠层程序从材质数据选择实际基底, 专用 IOR/specular 角色固定其基底.
bool UsesIorSpecular(PixelParams pixel)
{
#if defined(PHYSARA_CLEARCOAT) || defined(PHYSARA_SHEEN)
    return pixel.iorSpecularBase;
#elif defined(PHYSARA_IOR_SPECULAR)
    return true;
#else
    return false;
#endif
}

// 从已过滤的表面恢复核心波瓣, 屏幕解码与几何颜色入口共享此步.
PixelParams CorePixelParams(vec3 baseColor, float metallic, float filteredRoughness)
{
    return PixelParams(baseColor * (1.0 - metallic),
                       mix(vec3(0.04), baseColor, metallic),
                       1.0, baseColor, vec3(0.04), 1.0, metallic,
                       filteredRoughness, filteredRoughness * filteredRoughness
#if defined(PHYSARA_CLEARCOAT) || defined(PHYSARA_SHEEN)
                       , false
#endif
#ifdef PHYSARA_CLEARCOAT
                       , 0.0, 0.0, 0.0
#endif
#ifdef PHYSARA_SHEEN
                       , vec3(0.0), MinPerceptualRoughness, MinPerceptualRoughness * MinPerceptualRoughness, 0.0, 0.0
#endif
                       );
}

// 从已取样的通道构造波瓣. 扩展保留未加权的介质端点和独立强度, 供逐灯及 DFG 分别求值.
PixelParams BuildPixelParams(MaterialInputs inputs, float kernel)
{
    float roughness = FilterCoreRoughness(inputs.perceptualRoughness, kernel);
    PixelParams pixel = CorePixelParams(inputs.baseColor.rgb, inputs.metallic, roughness);
#if defined(PHYSARA_CLEARCOAT) || defined(PHYSARA_SHEEN)
    pixel.iorSpecularBase = inputs.iorSpecularBase;
#endif
#ifdef PHYSARA_CLEARCOAT
    pixel.coatWeight = inputs.coatStrength;
    pixel.coatRoughness = FilterCoreRoughness(inputs.coatRoughness, kernel);
    pixel.coatAlpha = pixel.coatRoughness * pixel.coatRoughness;
#endif
#ifdef PHYSARA_SHEEN
    pixel.sheenColor = inputs.sheenColor;
    pixel.sheenRoughness = FilterCoreRoughness(inputs.sheenRoughness, kernel);
    pixel.sheenAlpha = pixel.sheenRoughness * pixel.sheenRoughness;
#endif
#ifdef PHYSARA_HAS_SPECULAR_INPUTS
    if (UsesIorSpecular(pixel))
    {
        // IOR 零值表示兼容模式. 先限制有色介质 F0, 镜面强度在求值时进入 Fresnel.
        float reflectance = 1.0;
        if (inputs.ior != 0.0)
        {
            float amplitude = (inputs.ior - 1.0) / (inputs.ior + 1.0);
            reflectance = amplitude * amplitude;
        }
        pixel.dielectricF0 = min(vec3(1.0), reflectance * inputs.specularColor);
        pixel.specularWeight = inputs.specularWeight;
    }
#endif
    return pixel;
}

#ifdef PHYSARA_CLEARCOAT
const float CoatF0 = 0.04;

// Khronos 的视向层权重. 同一结果用于逐灯, 环境和基底发射.
void PrepareClearcoatWeight(inout PixelParams pixel, vec3 normal, vec3 viewDirection)
{
    float grazing = 1.0 - clamp(abs(dot(normal, viewDirection)), 0.0, 1.0);
    float square = grazing * grazing;
    pixel.coatWeight *= CoatF0 + (1.0 - CoatF0) * square * square * grazing;
}
#endif

#endif
