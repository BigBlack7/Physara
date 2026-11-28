#ifndef PHYSARA_OPAQUE_LIGHTING_GLSL
#define PHYSARA_OPAQUE_LIGHTING_GLSL

#include <Source/Common/UnitDirection.glsl>
#ifdef PHYSARA_FORWARD_FROXELS
#include <Source/Lighting/FroxelLighting.glsl>
#else
#include <Source/Lighting/Punctual.glsl>
#endif
#ifdef PHYSARA_PHYSICAL_LIGHTING
#include <Source/Lighting/Environment.glsl>
#endif

// 屏幕效果关闭时没有对应纹理或历史依赖. 反射辐亮度的输入已预曝光并预乘置信度.
struct ScreenLighting
{
    float ao;
    float reflectionConfidence;
    vec3 preExposedReflection;
};

const ScreenLighting NoScreenLighting = ScreenLighting(1.0, 0.0, vec3(0.0));

#ifdef PHYSARA_SHEEN
// Direct, Physical 和 IBL 共用 DFG 蓝通道. 以 sheen 自身的粗糙度查一次, 供整段灯表和环境项复用.
// 背向几何或着色法线时保留 PixelParams 的零初值, 基底透过率为 1.
void PrepareSheenResponse(inout PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection)
{
    if (MaxReflectance(pixel.sheenColor) > 0.0 && dot(geometry.facingGeometricNormal, viewDirection) > 0.0 &&
        dot(geometry.shadingNormal, viewDirection) > 0.0)
    {
        float noV = CoreNormalViewCosine(geometry.shadingNormal, viewDirection);
        // 反解 BakeDfg 蓝通道的平方网格, 再把两端样点对齐到首末 texel 中心. R/G 沿用各自的 UV.
        float p = (pixel.sheenRoughness - MinPerceptualRoughness) / (1.0 - MinPerceptualRoughness);
        vec2 index = sqrt(clamp(vec2(noV, p), 0.0, 1.0));
        const float dfgEdge = 128.0; // EnvironmentBake.hpp 中 DfgEdge 的固定产品尺寸.
        vec2 uv = (0.5 + index * (dfgEdge - 1.0)) / dfgEdge;
        float rawAlbedo = textureLod(sampler2D(dfgImage, environmentSampler), uv, 0.0).b;
        pixel.sheenScale = 1.0 / max(1.0, rawAlbedo);
        pixel.sheenAlbedo = min(rawAlbedo, 1.0);
    }
}
#endif

// 材质和屏幕入口共用 DFG, 逐灯累加, 环境响应及最终工作色限幅.
vec3 EvaluateOpaqueLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                            vec3 preExposedEmission, float materialAo, ScreenLighting screen
#ifdef PHYSARA_FORWARD_FROXELS
                            , uint cell
#endif
                            )
{
#ifdef PHYSARA_SHEEN
    PrepareSheenResponse(pixel, geometry, viewDirection);
#endif
    SpecularCompensation compensation = UnitSpecularCompensation;
#ifdef PHYSARA_PHYSICAL_LIGHTING
    LightingDfgTerms terms = PrepareLightingDfg(pixel, geometry, viewDirection);
    compensation = terms.directSpecular;
#endif
#ifdef PHYSARA_FORWARD_FROXELS
    vec3 result = EvaluateFroxelLighting(pixel, geometry, viewDirection, surface, preExposedEmission, compensation, cell);
#else
    vec3 result = EvaluateLighting(pixel, geometry, viewDirection, surface, preExposedEmission, compensation);
#endif
#if PHYSARA_PHYSICAL_LIGHTING == 2
    result += EvaluateIbl(pixel, geometry, viewDirection, min(materialAo, screen.ao), terms,
                          screen.reflectionConfidence, screen.preExposedReflection);
#endif
    return min(result, vec3(MaxWorkingLight));
}

#endif
