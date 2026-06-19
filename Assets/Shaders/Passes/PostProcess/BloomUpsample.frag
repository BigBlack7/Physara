#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;

layout(std140, binding = PHYSARA_BINDING_POST_PROCESS_SETTINGS)uniform PostProcessSettingsBuffer
{
    vec4 uBloomParams;
    vec4 uFlags;
    vec4 uExposureParams;
    vec4 uAAParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(binding = PHYSARA_BINDING_SCENE_DEPTH_TEXTURE)uniform sampler2D uLowerBloom;

layout(location = 0)out vec4 outColor;

vec3 SampleLower(vec2 uv)
{
    return clamp(texture(uLowerBloom, uv).rgb, vec3(0.0), vec3(60000.0));
}

void main()
{
    vec2 texel = 1.0 / max(vec2(textureSize(uLowerBloom, 0)), vec2(1.0));
    vec3 lower = SampleLower(inUV) * 4.0;
    lower += (SampleLower(inUV + vec2(1.0, 0.0) * texel) +
              SampleLower(inUV + vec2(-1.0, 0.0) * texel) +
              SampleLower(inUV + vec2(0.0, 1.0) * texel) +
              SampleLower(inUV + vec2(0.0, -1.0) * texel)) * 2.0;
    lower += SampleLower(inUV + vec2(1.0, 1.0) * texel) +
             SampleLower(inUV + vec2(-1.0, 1.0) * texel) +
             SampleLower(inUV + vec2(1.0, -1.0) * texel) +
             SampleLower(inUV + vec2(-1.0, -1.0) * texel);
    lower *= 1.0 / 16.0;
    vec3 high = clamp(texture(uSceneColor, inUV).rgb, vec3(0.0), vec3(60000.0));
    outColor = vec4(high + lower * uBloomParams.w, 1.0);
}
