#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticCoverage.glsl>
#include <Source/Shadow/Directional.glsl>

// 色相显示选级, 明暗显示真实可见性. 诊断值沿现有线性显示, 不依赖曝光.
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
        LightingPosition(vLightingOffsetHigh, vLightingOffsetLow));
    LightingPosition camera = LightingPosition(cascades.cameraOriginHigh, cascades.cameraOriginLow);
    float viewDepth = -dot(cascades.cameraForward, LightingDelta(camera, surface, 1.0));
    CascadeSelection selection = SelectCascade(viewDepth);
    if (selection.first == cascades.count)
    {
        // 有限深度范围外用中性灰, 此路径无对应级联纹理读取.
        outColor = vec4(vec3(0.35), 1.0);
        return;
    }
    vec3 color = palette[selection.first];
    float visibility = DirectionalVisibility(selection.first, surface);
    if (selection.nextWeight > 0.0)
    {
        // 两个投影各做一次常规深度比较, 再按同一个米制视深权重混合色相与可见性.
        color = mix(color, palette[selection.first + 1u], selection.nextWeight);
        visibility = mix(visibility, DirectionalVisibility(selection.first + 1u, surface), selection.nextWeight);
    }
    outColor = vec4(color * mix(0.25, 1.0, visibility), 1.0);
}
