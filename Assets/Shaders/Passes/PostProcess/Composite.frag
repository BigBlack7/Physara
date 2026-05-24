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
    vec4 uAAParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(binding = PHYSARA_BINDING_SCENE_DEPTH_TEXTURE)uniform sampler2D uSceneDepth;
layout(binding = PHYSARA_BINDING_BLOOM_TEXTURE)uniform sampler2D uBloomTexture;

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
    bvec3 invalid = bvec3(isnan(value.r) || isinf(value.r), isnan(value.g) || isinf(value.g), isnan(value.b) || isinf(value.b));
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
        for (int i = 0; i < 16; ++i)
        {
            luminanceSum += max(Luminance(SampleHDR(samples[i])), PHYSARA_EPSILON);
        }
        float averageLuminance = luminanceSum / 16.0;
        ev100 = log2(max(averageLuminance / (0.18 * 1.2), PHYSARA_EPSILON));
    }
    return ExposureFromEV100(ev100 - uExposureParams.y);
}

vec3 LegacyBloomContribution(vec2 uv)
{
    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    float threshold = uBloomParams.x;
    float knee = max(uBloomParams.y, 0.0001);
    float radius = max(uBloomParams.w, 1.0);
    float exposure = ResolveExposure();

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * texel * radius;
            vec3 sampleColor = SampleHDR(uv + offset);
            vec3 exposedSample = sampleColor * exposure;
            float brightness = max(max(exposedSample.r, exposedSample.g), exposedSample.b);
            float soft = clamp((brightness - threshold + knee) / (2.0 * knee), 0.0, 1.0);
            float contribution = max(brightness - threshold, 0.0) + soft * soft * knee;
            float thresholdWeight = contribution / max(brightness, PHYSARA_EPSILON);
            float gaussianWeight = 1.0 / (1.0 + dot(vec2(x, y), vec2(x, y)));
            sum += sampleColor * thresholdWeight * gaussianWeight;
            weightSum += thresholdWeight * gaussianWeight;
        }
    }

    return weightSum > 0.0 ? sum / weightSum : vec3(0.0);
}

vec3 ResolveMappedColor(vec2 uv)
{
    vec3 hdrColor = SampleHDR(uv);
    if (uFlags.y > 0.5)
    {
        float bloomMode = uExposureParams.z;
        vec3 bloom = bloomMode < 0.5 ? LegacyBloomContribution(uv) : SanitizeHDR(texture(uBloomTexture, uv).rgb);
        hdrColor += bloom * uBloomParams.z;
    }
    vec3 exposed = hdrColor * ResolveExposure();
    return uFlags.x > 0.5 ? TonemapACES(exposed) : clamp(exposed, 0.0, 1.0);
}

float MappedLuma(vec2 uv)
{
    return Luminance(ResolveMappedColor(uv));
}

vec3 ApplyBasicFXAA(vec2 uv, vec3 center)
{
    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    vec3 north = ResolveMappedColor(uv + vec2(0.0, texel.y));
    vec3 south = ResolveMappedColor(uv - vec2(0.0, texel.y));
    vec3 east = ResolveMappedColor(uv + vec2(texel.x, 0.0));
    vec3 west = ResolveMappedColor(uv - vec2(texel.x, 0.0));

    float lumaCenter = Luminance(center);
    float lumaMin = min(lumaCenter, min(min(Luminance(north), Luminance(south)), min(Luminance(east), Luminance(west))));
    float lumaMax = max(lumaCenter, max(max(Luminance(north), Luminance(south)), max(Luminance(east), Luminance(west))));
    float edge = lumaMax - lumaMin;
    if (edge < max(uAAParams.z, lumaMax * uAAParams.y))
    {
        return center;
    }

    return mix((north + south + east + west + center * 2.0) / 6.0, center, 1.0 - uAAParams.x);
}

vec3 ApplyQualityFXAA(vec2 uv, vec3 center)
{
    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    vec3 rgbNW = ResolveMappedColor(uv + vec2(-1.0, 1.0) * texel);
    vec3 rgbNE = ResolveMappedColor(uv + vec2(1.0, 1.0) * texel);
    vec3 rgbSW = ResolveMappedColor(uv + vec2(-1.0, -1.0) * texel);
    vec3 rgbSE = ResolveMappedColor(uv + vec2(1.0, -1.0) * texel);

    float lumaNW = Luminance(rgbNW);
    float lumaNE = Luminance(rgbNE);
    float lumaSW = Luminance(rgbSW);
    float lumaSE = Luminance(rgbSE);
    float lumaM = Luminance(center);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(uAAParams.z, lumaMax * uAAParams.y))
    {
        return center;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    vec3 rgbA = 0.5 * (
        ResolveMappedColor(uv + dir * (1.0 / 3.0 - 0.5)) +
        ResolveMappedColor(uv + dir * (2.0 / 3.0 - 0.5)));
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        ResolveMappedColor(uv + dir * -0.5) +
        ResolveMappedColor(uv + dir * 0.5));
    float lumaB = Luminance(rgbB);
    vec3 filtered = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    return mix(center, filtered, uAAParams.x);
}

vec3 ApplySMAALite(vec2 uv, vec3 center)
{
    vec2 texel = 1.0 / max(uViewportSizeEV100.xy, vec2(1.0));
    float c = Luminance(center);
    float l = MappedLuma(uv - vec2(texel.x, 0.0));
    float r = MappedLuma(uv + vec2(texel.x, 0.0));
    float u = MappedLuma(uv + vec2(0.0, texel.y));
    float d = MappedLuma(uv - vec2(0.0, texel.y));
    float horizontal = abs(u - c) + abs(d - c);
    float vertical = abs(l - c) + abs(r - c);
    float edge = max(horizontal, vertical);
    if (edge < max(uAAParams.z, c * uAAParams.y))
    {
        return center;
    }

    float depthC = texture(uSceneDepth, uv).r;
    float depthL = texture(uSceneDepth, uv - vec2(texel.x, 0.0)).r;
    float depthR = texture(uSceneDepth, uv + vec2(texel.x, 0.0)).r;
    float depthU = texture(uSceneDepth, uv + vec2(0.0, texel.y)).r;
    float depthD = texture(uSceneDepth, uv - vec2(0.0, texel.y)).r;
    float depthEdge = max(max(abs(depthC - depthL), abs(depthC - depthR)), max(abs(depthC - depthU), abs(depthC - depthD)));
    float depthWeight = clamp(depthEdge * uAAParams.w, 0.0, 1.0);

    vec3 blendA = ResolveMappedColor(uv + vec2(texel.x, 0.0));
    vec3 blendB = ResolveMappedColor(uv - vec2(texel.x, 0.0));
    if (horizontal > vertical)
    {
        blendA = ResolveMappedColor(uv + vec2(0.0, texel.y));
        blendB = ResolveMappedColor(uv - vec2(0.0, texel.y));
    }

    vec3 filtered = center * 0.5 + (blendA + blendB) * 0.25;
    return mix(center, filtered, clamp(uAAParams.x + depthWeight * 0.25, 0.0, 1.0));
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

    vec3 mapped = ResolveMappedColor(inUV);
    uint aaMode = uint(uFlags.z + 0.5);
    if (aaMode == 1u)
    {
        mapped = ApplyBasicFXAA(inUV, mapped);
    }
    else if (aaMode == 2u)
    {
        mapped = ApplyQualityFXAA(inUV, mapped);
    }
    else if (aaMode == 3u)
    {
        mapped = ApplySMAALite(inUV, mapped);
    }
    outColor = vec4(LinearToSrgb(mapped), 1.0);
}