// 各向同性 GGX 的单次散射与匹配 DFG 能量项, 只接收参数, 不访问灯或资源绑定.
// 采用 FP32 和完整 height-correlated Smith; 算法依据见 Docs/Reference/Filament/Lighting.md 11.3/11.7.
// 多散射与间接漫反射预算沿用其中的实时近似, 不承诺任意材质组合严格能量守恒.
#include <Source/Common/UnitDirection.glsl>
#include <Source/Material/CoreSurface.glsl>

// 单次散射的 BRDF 值以 sr^-1 表达, 不含光强, AO, 曝光及镜面多散射补偿.
// 各项已含对应的金属度权重. 调用方对每个镜面项施加自己的补偿, 合并后乘一次入射余弦.
struct CoreBrdfLobes
{
    vec3 diffuse;
    vec3 specular;
    vec3 metalSpecular;
};

// 两个材质 profile 共用逐灯接口. 扩展的介质与金属先各自补偿, 再合并镜面贡献.
struct SpecularCompensation
{
    vec3 base;
    vec3 metal;
#ifdef PHYSARA_CLEARCOAT
    float coat;
#endif
};

const SpecularCompensation UnitSpecularCompensation = SpecularCompensation(vec3(1.0), vec3(1.0)
#ifdef PHYSARA_CLEARCOAT
    , 1.0
#endif
);

// 用反射最强的颜色分量确定介质的标量漫反射预算, 供直接与间接光共同使用.
float MaxReflectance(vec3 value)
{
    return max(max(value.r, value.g), value.b);
}

// 输入为同一空间的单位着色法线和指向眼点的单位视向, 返回限制到 [1e-4, 1] 的 NoV.
// 核心波瓣与 DFG 查表共用此值, 保持掠射附近的模型和能量补偿一致.
float CoreNormalViewCosine(vec3 normal, vec3 viewDirection)
{
    return clamp(dot(normal, viewDirection), 1e-4, 1.0);
}

// GGX 与预积分 DFG 使用相同的完整 height-correlated Smith. NoL 和 NoH 由各波瓣的调用方裁定.
float IsotropicMicrofacet(vec3 normal, vec3 halfDirection, float alpha, float noH, float noV, float noL)
{
    const float inversePi = 0.3183098861837907;
    // 叉积直接求 1-NoH^2, 保留窄高光附近的有效位.
    vec3 normalCrossHalf = cross(normal, halfDirection);
    float projectedAlpha = noH * alpha;
    float distributionRoot = alpha / (dot(normalCrossHalf, normalCrossHalf) + projectedAlpha * projectedAlpha);
    float distribution = distributionRoot * distributionRoot * inversePi;
    float alphaSquared = alpha * alpha;
    float lambdaV = noL * sqrt(alphaSquared + (1.0 - alphaSquared) * noV * noV);
    float lambdaL = noV * sqrt(alphaSquared + (1.0 - alphaSquared) * noL * noL);
    return distribution * (0.5 / (lambdaV + lambdaL));
}

// pixel 来自 BuildPixelParams; 法线和两方向均为有限单位向量且处于同一空间.
// 视向指向眼点, 入射方向指向光源; NoL<=0 时所有波瓣均为零.
// 镜面只在 NoH>0 时求值. 扩展漫反射始终使用 LoH 的预算, 退化半角用零向量得到掠射预算.
// 本函数不应用 DFG 补偿.
// 依次计算半球/半角, GGX 分布, 完整 Smith visibility 和显式 F90 的 Schlick Fresnel.
CoreBrdfLobes EvaluateCoreBrdf(PixelParams pixel, vec3 normal, vec3 viewDirection, vec3 lightDirection)
{
    const float inversePi = 0.3183098861837907;
    CoreBrdfLobes lobes = CoreBrdfLobes(vec3(0.0), vec3(0.0), vec3(0.0));
    float noL = clamp(dot(normal, lightDirection), 0.0, 1.0);
    if (noL == 0.0)
    {
        return lobes;
    }

    vec3 halfDirection = UnitOr(viewDirection + lightDirection, vec3(0.0));
    float noH = min(dot(normal, halfDirection), 1.0);
    float grazing = 1.0 - clamp(dot(lightDirection, halfDirection), 0.0, 1.0);
    float grazingSquared = grazing * grazing;
    float fresnelWeight = grazingSquared * grazingSquared * grazing;
    lobes.diffuse = pixel.diffuseColor * inversePi;
    vec3 dielectric;
    if (UsesIorSpecular(pixel))
    {
        // 半角在着色法线下方时仍可有有效 LoH. 漫反射预算先于镜面可见性确定.
        dielectric = (pixel.dielectricF0 + (1.0 - pixel.dielectricF0) * fresnelWeight) * pixel.specularWeight;
        float diffuseBudget = 1.0 - MaxReflectance(dielectric);
        lobes.diffuse = pixel.baseColor * (inversePi * diffuseBudget * (1.0 - pixel.metallic));
    }
    if (noH <= 0.0)
    {
        return lobes;
    }

    float noV = CoreNormalViewCosine(normal, viewDirection);
    float microfacet = IsotropicMicrofacet(normal, halfDirection, pixel.alpha, noH, noV, noL);
    if (UsesIorSpecular(pixel))
    {
        vec3 metal = pixel.baseColor + (1.0 - pixel.baseColor) * fresnelWeight;
        lobes.specular = dielectric * ((1.0 - pixel.metallic) * microfacet);
        lobes.metalSpecular = metal * (pixel.metallic * microfacet);
    }
    else
    {
        vec3 fresnel = pixel.f0 + (pixel.f90 - pixel.f0) * fresnelWeight;
        lobes.specular = fresnel * microfacet;
    }
    return lobes;
}

#ifdef PHYSARA_CLEARCOAT
// 上层镜面使用单位 Fresnel 的 GGX. 出射 Fresnel 已由表面的层权重携带.
float EvaluateCoatBrdf(PixelParams pixel, vec3 normal, vec3 viewDirection, vec3 lightDirection, float noL)
{
    vec3 halfDirection = UnitOr(viewDirection + lightDirection, vec3(0.0));
    float noH = min(dot(normal, halfDirection), 1.0);
    return noH > 0.0 ? IsotropicMicrofacet(normal, halfDirection, pixel.coatAlpha, noH,
                                          CoreNormalViewCosine(normal, viewDirection), noL) : 0.0;
}
#endif
// 各字段均为逐色分量的无量纲量. directionalAlbedo 是未补偿的 E, indirectDiffuseBudget=1-E.
// specularCompensation 是镜面乘数 C, 直接光使用 specular*C, 环境镜面使用 E*C, 均只乘一次.
struct CoreDfgTerms
{
    vec3 directionalAlbedo;
    vec3 specularCompensation;
    vec3 indirectDiffuseBudget;
};

// pixel 须遵守核心 F90=1 合同; dfg 为同一 CoreNormalViewCosine 和过滤后 p 对应的有效样点.
// R=B, G=A+B 且 G 为正有限数, 由匹配模型的 DFG 生产/发布边界保证, 此处不重新校验或钳位.
// 返回 E=mix(R,G,F0), C=1+F0*(1/G-1) 和 1-E, 不将补偿提前并入 E 或漫反射预算.
CoreDfgTerms EvaluateCoreDfg(PixelParams pixel, vec2 dfg)
{
    vec3 albedo = mix(vec3(dfg.r), vec3(dfg.g), pixel.f0);
    vec3 compensation = 1.0 + pixel.f0 * (1.0 / dfg.g - 1.0);
    return CoreDfgTerms(albedo, compensation, 1.0 - albedo);
}
