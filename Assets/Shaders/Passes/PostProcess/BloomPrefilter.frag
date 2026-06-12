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
layout(binding = PHYSARA_BINDING_EXPOSURE_TEXTURE)uniform sampler2D uExposureTexture;

layout(location = 0)out vec4 outColor;

vec3 SanitizeHDR(vec3 value)
{
    bvec3 invalid = bvec3(isnan(value.r) || isinf(value.r), isnan(value.g) || isinf(value.g), isnan(value.b) || isinf(value.b));
    return clamp(mix(value, vec3(0.0), invalid), vec3(0.0), vec3(60000.0));
}

float ResolveExposure()
{
    return max(texture(uExposureTexture, vec2(0.5)).r, 0.0);
}

float SoftThreshold(vec3 color)
{
    vec3 exposed = color * ResolveExposure();
    float brightness = max(max(exposed.r, exposed.g), exposed.b);
    float threshold = uBloomParams.x;
    float knee = max(uBloomParams.y, 0.0001);
    float soft = clamp((brightness - threshold + knee) / (2.0 * knee), 0.0, 1.0);
    float contribution = max(brightness - threshold, 0.0) + soft * soft * knee;
    return contribution / max(brightness, PHYSARA_EPSILON);
}

void main()
{
    vec2 texel = 1.0 / max(vec2(textureSize(uSceneColor, 0)), vec2(1.0));
    vec3 a = SanitizeHDR(texture(uSceneColor, inUV + vec2(-1.0, -1.0) * texel).rgb);
    vec3 b = SanitizeHDR(texture(uSceneColor, inUV + vec2(1.0, -1.0) * texel).rgb);
    vec3 c = SanitizeHDR(texture(uSceneColor, inUV + vec2(-1.0, 1.0) * texel).rgb);
    vec3 d = SanitizeHDR(texture(uSceneColor, inUV + vec2(1.0, 1.0) * texel).rgb);
    vec3 e = SanitizeHDR(texture(uSceneColor, inUV).rgb);
    vec3 color = (a + b + c + d) * 0.125 + e * 0.5;
    outColor = vec4(color * SoftThreshold(color), 1.0);
}