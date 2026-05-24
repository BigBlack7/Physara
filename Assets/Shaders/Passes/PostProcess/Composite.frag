#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform PostProcessFrameBuffer
{
    vec4 uViewportSizeEV100;
};

layout(std140, binding = PHYSARA_BINDING_POST_PROCESS_SETTINGS)uniform PostProcessSettingsBuffer
{
    vec4 uBloomParams;
    vec4 uFlags;
    vec4 uExposureParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(binding = PHYSARA_BINDING_SCENE_DEPTH_TEXTURE)uniform sampler2D uSceneDepth;

layout(location = 0)out vec4 outColor;

vec3 LinearToSrgb(vec3 value)
{
    return pow(max(value, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 TonemapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 SanitizeHDR(vec3 value)
{
    bvec3 invalid = bvec3(isnan(value.r)|| isinf(value.r), isnan(value.g)|| isinf(value.g), isnan(value.b)|| isinf(value.b));
    return clamp(mix(value, vec3(0.0), invalid), vec3(0.0), vec3(60000.0));
}

vec3 SampleHDR(vec2 uv)
{
    return SanitizeHDR(texture(uSceneColor, uv).rgb);
}

float ResolveExposure()
{
    float ev100 = uViewportSizeEV100.z;
    if (uExposureParams.x > 0.5)
    {
        const vec2 samples[16] =
        vec2[16](vec2(0.125, 0.125), vec2(0.375, 0.125), vec2(0.625, 0.125), vec2(0.875, 0.125),
        vec2(0.125, 0.375), vec2(0.375, 0.375), vec2(0.625, 0.375), vec2(0.875, 0.375),
        vec2(0.125, 0.625), vec2(0.375, 0.625), vec2(0.625, 0.625), vec2(0.875, 0.625),
        vec2(0.125, 0.875), vec2(0.375, 0.875), vec2(0.625, 0.875), vec2(0.875, 0.875));
        float luminanceSum = 0.0;
        for(int i = 0; i < 16; ++ i)
        {
            luminanceSum += max(Luminance(SampleHDR(samples[i])), PHYSARA_EPSILON);
        }
        float averageLuminance = luminanceSum / 16.0;
        ev100 = log2(max(averageLuminance / (0.18 * 1.2), PHYSARA_EPSILON));
    }
    return ExposureFromEV100(ev100 - uExposureParams.y);
}

vec3 BloomContribution(vec2 uv)
{
    if (uFlags.y < 0.5)
    {
        return vec3(0.0);
    }

    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    float threshold = uBloomParams.x;
    float knee = max(uBloomParams.y, 0.0001);
    float intensity = uBloomParams.z;
    float radius = max(uBloomParams.w, 1.0);
    float exposure = ResolveExposure();

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    for(int y = -2; y <= 2; ++ y)
    {
        for(int x = -2; x <= 2; ++ x)
        {
            vec2 offset = vec2(float(x), float(y)) * texel * radius;
            vec3 sampleColor = SampleHDR(uv + offset);
            vec3 exposedSample = sampleColor * exposure;
            float brightness = max(max(exposedSample.r, exposedSample.g), exposedSample.b);
            float soft = clamp((brightness - threshold + knee) / (2.0 * knee), 0.0, 1.0);
            float contribution = max(brightness - threshold, 0.0) + soft * soft * knee;
            float weight = contribution / max(brightness, PHYSARA_EPSILON);
            sum += sampleColor * weight;
            weightSum += weight;
        }
    }

    return weightSum > 0.0 ? sum / weightSum * intensity : vec3(0.0);
}

vec3 ResolveMappedColor(vec2 uv, bool includeBloom)
{
    vec3 hdrColor = SampleHDR(uv);
    if (includeBloom)
    {
        hdrColor += BloomContribution(uv);
    }
    vec3 exposed = hdrColor * ResolveExposure();
    return uFlags.x > 0.5 ? TonemapACES(exposed) : clamp(exposed, 0.0, 1.0);
}

vec3 ApplyFXAA(vec2 uv)
{
    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    vec3 center = ResolveMappedColor(uv, true);
    if (uFlags.z < 0.5)
    {
        return center;
    }

    vec3 north = ResolveMappedColor(uv + vec2(0.0, texel.y), false);
    vec3 south = ResolveMappedColor(uv - vec2(0.0, texel.y), false);
    vec3 east = ResolveMappedColor(uv + vec2(texel.x, 0.0), false);
    vec3 west = ResolveMappedColor(uv - vec2(texel.x, 0.0), false);

    float lumaCenter = Luminance(center);
    float lumaMin = min(lumaCenter, min(min(Luminance(north), Luminance(south)), min(Luminance(east), Luminance(west))));
    float lumaMax = max(lumaCenter, max(max(Luminance(north), Luminance(south)), max(Luminance(east), Luminance(west))));
    float edge = lumaMax - lumaMin;
    if (edge < max(0.0312, lumaMax * 0.125))
    {
        return center;
    }

    return (north + south + east + west + center * 2.0) / 6.0;
}

float LinearizeDepth(float depth)
{
    float nearClip = max(uViewportSizeEV100.w, 0.0001);
    float farClip = 1000.0;
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearClip * farClip) / max(farClip + nearClip - z * (farClip - nearClip), PHYSARA_EPSILON);
}

void main()
{
    uint debugView = uint(uFlags.w + 0.5);
    if (debugView == 1u)
    {
        outColor = vec4(SampleHDR(inUV), 1.0);
        return;
    }
    if (debugView == 2u)
    {
        float rawDepth = texture(uSceneDepth, inUV).r;
        float linearDepth = LinearizeDepth(rawDepth);
        float normalizedDepth = clamp(linearDepth / 100.0, 0.0, 1.0);
        outColor = vec4(vec3(pow(normalizedDepth, 0.45)), 1.0);
        return;
    }

    vec3 mapped = ApplyFXAA(inUV);
    outColor = vec4(LinearToSrgb(mapped), 1.0);
}