#include <Interface/Fragment.glsl>
#include <Source/Material/StaticCoverage.glsl>
#include <Source/Shadow/Local.glsl>

// 色相表示所查面, 明暗表示实际深度比较. 覆盖外沿线性诊断显示中性灰.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    if (PHYSARA_FORWARD_MATERIAL.masked != 0u)
    {
        ApplyCoverage(SampleBaseColor(gradients).a);
    }

    LightingSurface surface = LightingSurface(vLightingAnchor,
        LightingPosition(vLightingOffsetHigh, vLightingOffsetLow));
    LocalShadowResult shadow = QueryLocalShadow(surface);
    if (shadow.face < 0)
    {
        outColor = vec4(vec3(0.35), 1.0);
        return;
    }

    const vec3 palette[6] = vec3[6](vec3(0.94, 0.25, 0.21), vec3(0.90, 0.69, 0.12),
        vec3(0.21, 0.78, 0.38), vec3(0.22, 0.51, 0.94), vec3(0.75, 0.36, 0.85), vec3(0.16, 0.74, 0.78));
    outColor = vec4(palette[shadow.face] * mix(0.25, 1.0, shadow.visibility), 1.0);
}
