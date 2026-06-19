#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/FrameUniforms.glsl"
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
layout(binding = PHYSARA_BINDING_SCENE_DEPTH_TEXTURE)uniform sampler2D uSceneDepth;
layout(binding = PHYSARA_BINDING_BLOOM_TEXTURE)uniform sampler2D uBloomTexture;

layout(location = 0)out vec4 outColor;

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
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

vec3 TonemapReinhard(vec3 color)
{
    return color / (vec3(1.0) + color);
}

vec3 Uncharted2Curve(vec3 color)
{
    const float a = 0.15;
    const float b = 0.50;
    const float c = 0.10;
    const float d = 0.20;
    const float e = 0.02;
    const float f = 0.30;
    return ((color * (a * color + c * b) + d * e) / (color * (a * color + b) + d * f)) - e / f;
}

vec3 TonemapFilmic(vec3 color)
{
    const float whitePoint = 11.2;
    return clamp(Uncharted2Curve(color * 2.0) / Uncharted2Curve(vec3(whitePoint)), 0.0, 1.0);
}

vec3 TonemapNeutral(vec3 color)
{
    const float startCompression = 0.76;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color = max(color - offset, vec3(0.0));
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
    {
        return color;
    }

    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / max(peak + d - startCompression, PHYSARA_EPSILON);
    color *= newPeak / max(peak, PHYSARA_EPSILON);
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

vec3 ApplyToneMapping(vec3 color)
{
    uint mode = uint(uFlags.x + 0.5);
    if (mode == 1u)
    {
        return TonemapACES(color);
    }
    if (mode == 2u)
    {
        return TonemapReinhard(color);
    }
    if (mode == 3u)
    {
        return TonemapFilmic(color);
    }
    if (mode == 4u)
    {
        return TonemapNeutral(color);
    }
    return clamp(color, 0.0, 1.0);
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

float ResolveExposureAdjustment()
{
    return exp2(uExposureParams.x);
}

vec3 ResolveMappedColor(vec2 uv)
{
    vec3 hdrColor = SampleHDR(uv);
    if (uFlags.y > 0.5)
    {
        hdrColor += SanitizeHDR(texture(uBloomTexture, uv).rgb) * uBloomParams.z;
    }
    vec3 exposed = hdrColor * ResolveExposureAdjustment();
    return ApplyToneMapping(exposed);
}

float MappedLuma(vec2 uv)
{
    return Luminance(ResolveMappedColor(uv));
}

vec3 ApplyBasicFXAA(vec2 uv, vec3 center)
{
    vec2 texel = 1.0 / max(uFrame.camera.viewportRect.zw, vec2(1.0));
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
    vec2 texel = 1.0 / max(uFrame.camera.viewportRect.zw, vec2(1.0));
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
    vec2 texel = 1.0 / max(uFrame.camera.viewportRect.zw, vec2(1.0));
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
    float nearClip = max(uFrame.camera.clipPlanes.x, 0.0001);
    float farClip = max(uFrame.camera.clipPlanes.y, nearClip + 0.0001);
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
