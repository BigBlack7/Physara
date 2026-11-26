#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticCoverage.glsl>
#include <Source/Geometry/SurfaceFrame.glsl>
#include <Source/Shadow/Visibility.glsl>

// 色相显示选级, 明暗显示与普通光照共用的可见性.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    if (PHYSARA_FORWARD_MATERIAL.masked != 0u)
    {
        ApplyCoverage(SampleBaseColor(gradients).a);
    }

    const vec3 palette[4] = vec3[4](vec3(0.94, 0.25, 0.21), vec3(0.90, 0.69, 0.12),
                                   vec3(0.21, 0.78, 0.38), vec3(0.22, 0.51, 0.94));
    LightingSurface surface = LightingSurface(vLightingAnchor,
        LightingPosition(vLightingOffsetHigh, vLightingOffsetLow, vec3(0.0)));
    ShadowResult shadow = QueryShadow(1u, surface, GeometricNormal());
    if (shadow.subview < 0)
    {
        outColor = vec4(vec3(0.35), 1.0);
        return;
    }
    vec3 color = palette[shadow.subview];
    if (shadow.nextWeight > 0.0)
    {
        color = mix(color, palette[shadow.subview + 1], shadow.nextWeight);
    }
    outColor = vec4(color * mix(0.25, 1.0, shadow.visibility), 1.0);
}
