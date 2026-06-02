#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform PostProcessFrameBuffer
{
    vec4 uViewportSizeEV100;
    vec4 uClipPlanes;
};

layout(std140, binding = PHYSARA_BINDING_POST_PROCESS_SETTINGS)uniform PostProcessSettingsBuffer
{
    vec4 uBloomParams;
    vec4 uFlags;
    vec4 uExposureParams;
    vec4 uAAParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;

layout(location = 0)out vec4 outColor;

vec3 SanitizeHDR(vec3 value)
{
    bvec3 invalid = bvec3(isnan(value.r) || isinf(value.r), isnan(value.g) || isinf(value.g), isnan(value.b) || isinf(value.b));
    return clamp(mix(value, vec3(0.0), invalid), vec3(0.0), vec3(60000.0));
}

float ResolveEV100()
{
    float ev100 = uViewportSizeEV100.z;
    if (uExposureParams.x <= 0.5)
    {
        return ev100;
    }

    const vec2 samples[16] =
    vec2[16](vec2(0.125, 0.125), vec2(0.375, 0.125), vec2(0.625, 0.125), vec2(0.875, 0.125),
    vec2(0.125, 0.375), vec2(0.375, 0.375), vec2(0.625, 0.375), vec2(0.875, 0.375),
    vec2(0.125, 0.625), vec2(0.375, 0.625), vec2(0.625, 0.625), vec2(0.875, 0.625),
    vec2(0.125, 0.875), vec2(0.375, 0.875), vec2(0.625, 0.875), vec2(0.875, 0.875));
    float luminanceSum = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        luminanceSum += max(Luminance(SanitizeHDR(texture(uSceneColor, samples[i]).rgb)), PHYSARA_EPSILON);
    }

    float averageLuminance = luminanceSum / 16.0;
    return log2(max(averageLuminance / (0.18 * 1.2), PHYSARA_EPSILON));
}

void main()
{
    float exposure = ExposureFromEV100(ResolveEV100() - uExposureParams.y);
    outColor = vec4(exposure, 0.0, 0.0, 1.0);
}