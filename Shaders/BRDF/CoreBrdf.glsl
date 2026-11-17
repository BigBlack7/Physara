// 核心各向同性 GGX 的单次散射与匹配 DFG 能量项, 只接收参数, 不访问灯或资源绑定.
// 采用 FP32 和完整 height-correlated Smith; 算法依据见 Docs/Reference/Filament/Lighting.md 11.3/11.7.
// 多散射与间接漫反射预算沿用其中的实时近似, 不承诺任意材质组合严格能量守恒.
#include <Source/Common/UnitDirection.glsl>
#include <Source/Material/CoreSurface.glsl>

// 单次散射的 BRDF 值以 sr^-1 表达, 不含光强, AO, 曝光及镜面多散射补偿.
// 调用方对 diffuse + specular 乘一次入射余弦; 补偿只对 specular 应用一次.
struct CoreBrdfLobes
{
    vec3 diffuse;
    vec3 specular;
};

// 输入为同一空间的单位着色法线和指向眼点的单位视向, 返回限制到 [1e-4, 1] 的 NoV.
// 核心波瓣与 DFG 查表共用此值, 保持掠射附近的模型和能量补偿一致.
float CoreNormalViewCosine(vec3 normal, vec3 viewDirection)
{
    return clamp(dot(normal, viewDirection), 1e-4, 1.0);
}

// pixel 来自 BuildPixelParams; 法线和两方向均为有限单位向量且处于同一空间.
// 视向指向眼点, 入射方向指向光源; NoL<=0 时两个波瓣均为零.
// 未定义或位于着色半球外的半角仅使镜面归零, 仍保留 Lambert; 本函数不应用 DFG 补偿.
// 依次计算半球/半角, GGX 分布, 完整 Smith visibility 和显式 F90 的 Schlick Fresnel.
CoreBrdfLobes EvaluateCoreBrdf(PixelParams pixel, vec3 normal, vec3 viewDirection, vec3 lightDirection)
{
    const float inversePi = 0.3183098861837907;
    float noL = clamp(dot(normal, lightDirection), 0.0, 1.0);
    if (noL == 0.0)
    {
        return CoreBrdfLobes(vec3(0.0), vec3(0.0));
    }

    vec3 diffuse = pixel.diffuseColor * inversePi;
    vec3 halfDirection = UnitOr(viewDirection + lightDirection, vec3(0.0));
    float noH = min(dot(normal, halfDirection), 1.0);
    if (noH <= 0.0)
    {
        return CoreBrdfLobes(diffuse, vec3(0.0));
    }

    // 单位方向的叉积平方给出 1 - NoH^2, 避免窄高光处的相减消去误差.
    // 此 FP32 profile 保留粗糙度下限处的峰值, 不采用 Filament 的 FP16 峰值截断.
    vec3 normalCrossHalf = cross(normal, halfDirection);
    float projectedAlpha = noH * pixel.alpha;
    float distributionRoot = pixel.alpha /
        (dot(normalCrossHalf, normalCrossHalf) + projectedAlpha * projectedAlpha);
    float distribution = distributionRoot * distributionRoot * inversePi;

    // 此 visibility 已含微表面分母. alpha 和 NoV 的正下限使分母保持正数.
    float noV = CoreNormalViewCosine(normal, viewDirection);
    float alphaSquared = pixel.alpha * pixel.alpha;
    float lambdaV = noL * sqrt(alphaSquared + (1.0 - alphaSquared) * noV * noV);
    float lambdaL = noV * sqrt(alphaSquared + (1.0 - alphaSquared) * noL * noL);
    float visibility = 0.5 / (lambdaV + lambdaL);

    float grazing = 1.0 - clamp(dot(lightDirection, halfDirection), 0.0, 1.0);
    float grazingSquared = grazing * grazing;
    vec3 fresnel = pixel.f0 + (pixel.f90 - pixel.f0) * (grazingSquared * grazingSquared * grazing);
    return CoreBrdfLobes(diffuse, fresnel * (distribution * visibility));
}

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
