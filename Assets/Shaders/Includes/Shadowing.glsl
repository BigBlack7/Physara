#ifndef PHYSARA_SHADOWING_GLSL
#define PHYSARA_SHADOWING_GLSL

#include "Lighting.glsl"

layout(binding = PHYSARA_BINDING_SHADOW_MAP)uniform sampler2DArray uShadowMap;

const vec2 kShadowPoissonDisk16[16] = vec2[16](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790));

float CompareShadowDepth(vec2 uv, float receiverDepth, int cascadeIndex)
{
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }
    float closestDepth = texture(uShadowMap, vec3(uv, float(cascadeIndex))).r;
    return receiverDepth <= closestDepth ? 1.0 : 0.0;
}

float SampleShadowGrid(vec2 uv, float receiverDepth, int cascadeIndex, int radius, float radiusTexels)
{
    float visibility = 0.0;
    float sampleCount = 0.0;
    float texel = max(uFrame.shadow.params.w, 1.0 / max(uFrame.shadow.params.y, 1.0));
    float scale = max(radiusTexels, 0.25) / max(float(radius), 1.0);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            visibility += CompareShadowDepth(
                uv + vec2(float(x), float(y)) * texel * scale,
                receiverDepth,
                cascadeIndex);
            sampleCount += 1.0;
        }
    }
    return visibility / max(sampleCount, 1.0);
}

float SampleShadowPoisson16(vec2 uv, float receiverDepth, int cascadeIndex, float radiusTexels)
{
    float visibility = 0.0;
    float texel = max(uFrame.shadow.params.w, 1.0 / max(uFrame.shadow.params.y, 1.0));
    float radius = max(radiusTexels, 0.25) * texel;
    for (int i = 0; i < 16; ++i)
    {
        visibility += CompareShadowDepth(uv + kShadowPoissonDisk16[i] * radius, receiverDepth, cascadeIndex);
    }
    return visibility / 16.0;
}

float FindAverageShadowBlockerDepth(
    vec2 uv,
    float receiverDepth,
    int cascadeIndex,
    float searchRadiusTexels)
{
    float blockerDepthSum = 0.0;
    float blockerCount = 0.0;
    float texel = max(uFrame.shadow.params.w, 1.0 / max(uFrame.shadow.params.y, 1.0));
    float radius = max(searchRadiusTexels, 0.25) * texel;
    for (int i = 0; i < 16; ++i)
    {
        vec2 sampleUV = uv + kShadowPoissonDisk16[i] * radius;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            continue;
        }
        float depth = texture(uShadowMap, vec3(sampleUV, float(cascadeIndex))).r;
        if (depth < receiverDepth)
        {
            blockerDepthSum += depth;
            blockerCount += 1.0;
        }
    }
    return blockerCount > 0.0 ? blockerDepthSum / blockerCount : -1.0;
}

float SampleShadowPCSS(
    vec2 uv,
    float receiverDepth,
    int cascadeIndex,
    float filterRadiusTexels,
    float lightSizeTexels)
{
    float blockerDepth = FindAverageShadowBlockerDepth(
        uv,
        receiverDepth,
        cascadeIndex,
        max(lightSizeTexels * 0.5, filterRadiusTexels));
    if (blockerDepth < 0.0)
    {
        return 1.0;
    }
    float penumbraRatio = clamp(
        (receiverDepth - blockerDepth) / max(blockerDepth, PHYSARA_EPSILON),
        0.0,
        1.0);
    float penumbraRadius = clamp(
        filterRadiusTexels + penumbraRatio * lightSizeTexels,
        filterRadiusTexels,
        max(lightSizeTexels, filterRadiusTexels));
    return SampleShadowPoisson16(uv, receiverDepth, cascadeIndex, penumbraRadius);
}

bool BuildCascadeShadowCoordinates(
    vec3 worldPosition,
    vec3 normal,
    LightData light,
    int cascadeIndex,
    out vec3 shadowCoord)
{
    vec3 lightDirection = SafeNormalize(light.directionType.xyz);
    float NoL = Saturate(dot(normal, -lightDirection));
    float sinTheta = sqrt(max(1.0 - NoL * NoL, 0.0));
    float texelWorldSize = uFrame.shadow.cascadeTexelWorldSize[cascadeIndex];
    float normalOffset = texelWorldSize * max(uFrame.shadow.controls.z, 0.0) * sinTheta;
    vec3 biasedWorldPosition = worldPosition + normal * normalOffset;
    vec4 lightClip = uFrame.shadow.lightViewProjection[cascadeIndex] * vec4(biasedWorldPosition, 1.0);
    shadowCoord = lightClip.xyz / max(abs(lightClip.w), PHYSARA_EPSILON);
    shadowCoord = shadowCoord * 0.5 + 0.5;
    return shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 &&
           shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 &&
           shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0;
}

float SampleCascadeShadow(vec3 worldPosition, vec3 normal, LightData light, int cascadeIndex)
{
    vec3 shadowCoord;
    if (!BuildCascadeShadowCoordinates(worldPosition, normal, light, cascadeIndex, shadowCoord))
    {
        return 1.0;
    }
    float receiverBias = max(light.shadowParams.y, 0.0) * max(uFrame.shadow.controls.w, 0.0);
    float receiverDepth = clamp(shadowCoord.z - receiverBias, 0.0, 1.0);
    float filterRadius = max(uFrame.shadow.samplingParams.x, 0.25);
    float lightSize = max(uFrame.shadow.samplingParams.y, 1.0);
    uint filterMode = uint(uFrame.shadow.samplingParams.z + 0.5);
    if (filterMode == PHYSARA_SHADOW_FILTER_HARD)
    {
        return CompareShadowDepth(shadowCoord.xy, receiverDepth, cascadeIndex);
    }
    if (filterMode == PHYSARA_SHADOW_FILTER_PCF_5X5)
    {
        return SampleShadowGrid(shadowCoord.xy, receiverDepth, cascadeIndex, 2, filterRadius);
    }
    if (filterMode == PHYSARA_SHADOW_FILTER_POISSON_16)
    {
        return SampleShadowPoisson16(shadowCoord.xy, receiverDepth, cascadeIndex, filterRadius);
    }
    if (filterMode == PHYSARA_SHADOW_FILTER_PCSS)
    {
        return SampleShadowPCSS(shadowCoord.xy, receiverDepth, cascadeIndex, filterRadius, lightSize);
    }
    return SampleShadowGrid(shadowCoord.xy, receiverDepth, cascadeIndex, 1, filterRadius);
}

int SelectShadowCascade(float viewDepth, int cascadeCount)
{
    for (int cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        if (viewDepth <= uFrame.shadow.cascadeSplits[cascadeIndex])
        {
            return cascadeIndex;
        }
    }
    return -1;
}

float SampleShadow(
    vec3 worldPosition,
    vec3 normal,
    LightData light,
    uint lightIndex,
    bool receivesShadow)
{
    if (uFrame.shadow.params.x < 0.5 ||
        uint(uFrame.shadow.params.z + 0.5) != lightIndex ||
        !receivesShadow)
    {
        return 1.0;
    }
    int cascadeCount = clamp(int(uFrame.shadow.controls.x + 0.5), 1, PHYSARA_MAX_SHADOW_CASCADES);
    float viewDepth = max(-(uFrame.camera.view * vec4(worldPosition, 1.0)).z, 0.0);
    int cascadeIndex = SelectShadowCascade(viewDepth, cascadeCount);
    if (cascadeIndex < 0)
    {
        return 1.0;
    }
    float visibility = SampleCascadeShadow(worldPosition, normal, light, cascadeIndex);
    if (cascadeIndex + 1 < cascadeCount)
    {
        float cascadeNear = cascadeIndex == 0
                                ? uFrame.camera.clipPlanes.x
                                : uFrame.shadow.cascadeSplits[cascadeIndex - 1];
        float cascadeFar = uFrame.shadow.cascadeSplits[cascadeIndex];
        float transitionWidth = max((cascadeFar - cascadeNear) * uFrame.shadow.controls.y, PHYSARA_EPSILON);
        float transition = smoothstep(cascadeFar - transitionWidth, cascadeFar, viewDepth);
        if (transition > 0.0)
        {
            visibility = mix(
                visibility,
                SampleCascadeShadow(worldPosition, normal, light, cascadeIndex + 1),
                transition);
        }
    }
    return visibility;
}

#endif