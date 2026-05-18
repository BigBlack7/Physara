#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std140, binding = PHYSARA_BINDING_POST_PROCESS_SETTINGS)uniform PostProcessSettingsBuffer
{
    vec4 uBloomParams;
    vec4 uFlags;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;

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

vec3 SampleHDR(vec2 uv)
{
    return texture(uSceneColor, uv).rgb;
}

vec3 BloomContribution(vec2 uv)
{
    if (uFlags.y < 0.5)
    {
        return vec3(0.0);
    }

    vec2 texel = 1.0 / max(uCamera.viewportRect.zw, vec2(1.0));
    float threshold = uBloomParams.x;
    float knee = max(uBloomParams.y, 0.0001);
    float intensity = uBloomParams.z;
    float radius = max(uBloomParams.w, 1.0);

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * texel * radius;
            vec3 sampleColor = SampleHDR(uv + offset);
            float brightness = max(max(sampleColor.r, sampleColor.g), sampleColor.b);
            float soft = clamp((brightness - threshold + knee) / (2.0 * knee), 0.0, 1.0);
            float contribution = max(brightness - threshold, 0.0) + soft * soft * knee;
            float weight = contribution / max(brightness, PHYSARA_EPSILON);
            sum += sampleColor * weight;
            weightSum += weight;
        }
    }

    return weightSum > 0.0 ? sum / weightSum * intensity : vec3(0.0);
}

vec3 ResolveMappedColor(vec2 uv)
{
    vec3 hdrColor = SampleHDR(uv) + BloomContribution(uv);
    vec3 exposed = hdrColor * ExposureFromEV100(GetEV100(uCamera));
    return uFlags.x > 0.5 ? TonemapACES(exposed) : clamp(exposed, 0.0, 1.0);
}

vec3 ApplyFXAA(vec2 uv)
{
    vec2 texel = 1.0 / max(uCamera.viewportRect.zw, vec2(1.0));
    vec3 center = ResolveMappedColor(uv);
    if (uFlags.z < 0.5)
    {
        return center;
    }

    vec3 north = ResolveMappedColor(uv + vec2(0.0, texel.y));
    vec3 south = ResolveMappedColor(uv - vec2(0.0, texel.y));
    vec3 east = ResolveMappedColor(uv + vec2(texel.x, 0.0));
    vec3 west = ResolveMappedColor(uv - vec2(texel.x, 0.0));

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

void main()
{
    vec3 mapped = ApplyFXAA(inUV);
    outColor = vec4(LinearToSrgb(mapped), 1.0);
}