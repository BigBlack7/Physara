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

// 材质和屏幕入口共用 DFG, 逐灯累加, 环境响应及最终工作色限幅.
vec3 EvaluateOpaqueLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                            vec3 preExposedEmission, float materialAo, ScreenLighting screen
#ifdef PHYSARA_FORWARD_FROXELS
                            , uint cell
#endif
                            )
{
    vec3 compensation = vec3(1.0);
#ifdef PHYSARA_PHYSICAL_LIGHTING
    CoreDfgTerms terms = PrepareCoreDfg(pixel, geometry.shadingNormal, viewDirection);
    compensation = terms.specularCompensation;
#endif
#ifdef PHYSARA_FORWARD_FROXELS
    vec3 result = EvaluateFroxelLighting(pixel, geometry, viewDirection, surface, preExposedEmission, compensation, cell);
#else
    vec3 result = EvaluateLighting(pixel, geometry, viewDirection, surface, preExposedEmission, compensation);
#endif
#if PHYSARA_PHYSICAL_LIGHTING == 2
    result += EvaluateIbl(pixel, geometry.shadingNormal, viewDirection, min(materialAo, screen.ao), terms,
                          screen.reflectionConfidence, screen.preExposedReflection);
#endif
    return min(result, vec3(MaxWorkingLight));
}

#endif
