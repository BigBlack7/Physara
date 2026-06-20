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
    vec4 uDebugParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;

layout(location = 0)out vec4 outColor;

vec3 SanitizeHDR(vec3 value)
{
    bvec3 invalid = bvec3(isnan(value.r) || isinf(value.r), isnan(value.g) || isinf(value.g), isnan(value.b) || isinf(value.b));
    return clamp(mix(value, vec3(0.0), invalid), vec3(0.0), vec3(60000.0));
}

float ResolveExposureAdjustment()
{
    return exp2(uExposureParams.x);
}

vec3 KarisAverage(vec3 a, vec3 b, vec3 c, vec3 d)
{
    float exposure = ResolveExposureAdjustment();
    float wa = 1.0 / (1.0 + Luminance(a * exposure));
    float wb = 1.0 / (1.0 + Luminance(b * exposure));
    float wc = 1.0 / (1.0 + Luminance(c * exposure));
    float wd = 1.0 / (1.0 + Luminance(d * exposure));
    return (a * wa + b * wb + c * wc + d * wd) / max(wa + wb + wc + wd, PHYSARA_EPSILON);
}

float SoftThreshold(vec3 color)
{
    vec3 exposed = color * ResolveExposureAdjustment();
    float brightness = max(max(exposed.r, exposed.g), exposed.b);
    float threshold = uBloomParams.x;
    float knee = max(uBloomParams.y, 0.0001);
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + PHYSARA_EPSILON);
    float contribution = max(brightness - threshold, soft);
    return contribution / max(brightness, PHYSARA_EPSILON);
}

void main()
{
    vec2 texel = 1.0 / max(vec2(textureSize(uSceneColor, 0)), vec2(1.0));
    vec3 center = SanitizeHDR(texture(uSceneColor, inUV).rgb);
    vec3 left = SanitizeHDR(texture(uSceneColor, inUV + vec2(-2.0, 0.0) * texel).rgb);
    vec3 top = SanitizeHDR(texture(uSceneColor, inUV + vec2(0.0, 2.0) * texel).rgb);
    vec3 right = SanitizeHDR(texture(uSceneColor, inUV + vec2(2.0, 0.0) * texel).rgb);
    vec3 bottom = SanitizeHDR(texture(uSceneColor, inUV + vec2(0.0, -2.0) * texel).rgb);
    vec3 leftTop = SanitizeHDR(texture(uSceneColor, inUV + vec2(-1.0, 1.0) * texel).rgb);
    vec3 rightTop = SanitizeHDR(texture(uSceneColor, inUV + vec2(1.0, 1.0) * texel).rgb);
    vec3 rightBottom = SanitizeHDR(texture(uSceneColor, inUV + vec2(1.0, -1.0) * texel).rgb);
    vec3 leftBottom = SanitizeHDR(texture(uSceneColor, inUV + vec2(-1.0, -1.0) * texel).rgb);
    vec3 farLeftTop = SanitizeHDR(texture(uSceneColor, inUV + vec2(-2.0, 2.0) * texel).rgb);
    vec3 farRightTop = SanitizeHDR(texture(uSceneColor, inUV + vec2(2.0, 2.0) * texel).rgb);
    vec3 farRightBottom = SanitizeHDR(texture(uSceneColor, inUV + vec2(2.0, -2.0) * texel).rgb);
    vec3 farLeftBottom = SanitizeHDR(texture(uSceneColor, inUV + vec2(-2.0, -2.0) * texel).rgb);

    vec3 color = KarisAverage(leftTop, rightTop, rightBottom, leftBottom) * 0.5;
    color += KarisAverage(center, left, top, farLeftTop) * 0.125;
    color += KarisAverage(center, right, top, farRightTop) * 0.125;
    color += KarisAverage(center, right, bottom, farRightBottom) * 0.125;
    color += KarisAverage(center, left, bottom, farLeftBottom) * 0.125;
    outColor = vec4(color * SoftThreshold(color), 1.0);
}