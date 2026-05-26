#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Lighting.glsl"
#include "../../Includes/IBL.glsl"

layout(location = 0)in vec3 inWorldPosition;
layout(location = 1)in vec3 inWorldNormal;
layout(location = 2)in vec4 inWorldTangent;
layout(location = 3)in vec2 inTexCoord0;
layout(location = 4)in vec2 inTexCoord1;
layout(location = 5)flat in uint inMaterialIndex;
layout(location = 6)flat in uint inObjectFlags;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std430, binding = PHYSARA_BINDING_MATERIALS)readonly buffer MaterialBuffer
{
    MaterialData uMaterials[];
};

layout(std430, binding = PHYSARA_BINDING_LIGHTS)readonly buffer LightBuffer
{
    uint uLightCount;
    uint uLightPadding0;
    uint uLightPadding1;
    uint uLightPadding2;
    LightData uLights[];
};

layout(std140, binding = PHYSARA_BINDING_RENDER_SETTINGS)uniform RenderSettingsBuffer
{
    vec4 uDebugParams;
};

layout(std140, binding = PHYSARA_BINDING_SHADOW)uniform ShadowBuffer
{
    ShadowData uShadow;
};

layout(binding = PHYSARA_BINDING_BASE_COLOR_TEXTURE)uniform sampler2D uBaseColorTexture;
layout(binding = PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE)uniform sampler2D uMetallicRoughnessTexture;
layout(binding = PHYSARA_BINDING_NORMAL_TEXTURE)uniform sampler2D uNormalTexture;
layout(binding = PHYSARA_BINDING_OCCLUSION_TEXTURE)uniform sampler2D uOcclusionTexture;
layout(binding = PHYSARA_BINDING_EMISSIVE_TEXTURE)uniform sampler2D uEmissiveTexture;
layout(binding = PHYSARA_BINDING_SHADOW_MAP)uniform sampler2D uShadowMap;

layout(location = 0)out vec4 outColor;

vec3 SrgbToLinear(vec3 value)
{
    value = clamp(value, vec3(0.0), vec3(1.0));
    vec3 low = value / 12.92;
    vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), value));
}

vec3 ResolveWorldNormal(MaterialInputs inputs, vec3 geometricNormal, vec4 worldTangent, vec2 texCoord)
{
    if (!inputs.hasNormalTexture)
    {
        return normalize(geometricNormal);
    }
    
    vec3 n = normalize(geometricNormal);
    vec3 t = normalize(worldTangent.xyz - n * dot(n, worldTangent.xyz));
    vec3 b = normalize(cross(n, t) * worldTangent.w);
    mat3 tbn = mat3(t, b, n);
    vec3 tangentNormal = texture(uNormalTexture, texCoord).xyz * 2.0 - 1.0;
    if (inputs.flipNormalY)
    {
        tangentNormal.y = -tangentNormal.y;
    }
    tangentNormal.xy *= inputs.normalScale;
    return normalize(tbn * tangentNormal);
}

vec2 SelectTexCoord(uint texCoordSet)
{
    return texCoordSet == 1u ? inTexCoord1 : inTexCoord0;
}

const vec2 kPoissonDisk16[16] = vec2[16](
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
    vec2(0.14383161, -0.14100790)
);

float CompareShadowDepth(vec2 uv, float receiverDepth)
{
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }

    float closestDepth = texture(uShadowMap, uv).r;
    return receiverDepth <= closestDepth ? 1.0 : 0.0;
}

float SampleShadowGrid(vec2 uv, float receiverDepth, float texel, int radius, float filterRadiusTexels)
{
    float visibility = 0.0;
    float sampleCount = 0.0;
    float scale = filterRadiusTexels / max(float(radius), 1.0);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * texel * scale;
            visibility += CompareShadowDepth(uv + offset, receiverDepth);
            sampleCount += 1.0;
        }
    }
    return visibility / max(sampleCount, 1.0);
}

float SampleShadowPoisson16(vec2 uv, float receiverDepth, float texel, float radiusTexels)
{
    float visibility = 0.0;
    float radius = max(radiusTexels, 0.25) * texel;
    for (int i = 0; i < 16; ++i)
    {
        visibility += CompareShadowDepth(uv + kPoissonDisk16[i] * radius, receiverDepth);
    }
    return visibility / 16.0;
}

float FindAverageBlockerDepth(vec2 uv, float receiverDepth, float texel, float searchRadiusTexels)
{
    float averageBlocker = 0.0;
    float blockerCount = 0.0;
    float radius = max(searchRadiusTexels, 0.25) * texel;
    for (int i = 0; i < 16; ++i)
    {
        vec2 sampleUV = uv + kPoissonDisk16[i] * radius;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            continue;
        }

        float depth = texture(uShadowMap, sampleUV).r;
        if (depth < receiverDepth)
        {
            averageBlocker += depth;
            blockerCount += 1.0;
        }
    }

    if (blockerCount < 0.5)
    {
        return -1.0;
    }
    return averageBlocker / blockerCount;
}

float SampleShadowPCSS(vec2 uv, float receiverDepth, float texel, float filterRadiusTexels, float lightSizeTexels)
{
    float searchRadius = max(lightSizeTexels * 0.5, filterRadiusTexels);
    float blockerDepth = FindAverageBlockerDepth(uv, receiverDepth, texel, searchRadius);
    if (blockerDepth < 0.0)
    {
        return 1.0;
    }

    float penumbraRatio = clamp((receiverDepth - blockerDepth) / max(blockerDepth, PHYSARA_EPSILON), 0.0, 1.0);
    float penumbraTexels = clamp(filterRadiusTexels + penumbraRatio * lightSizeTexels, filterRadiusTexels, max(lightSizeTexels, filterRadiusTexels));
    return SampleShadowPoisson16(uv, receiverDepth, texel, penumbraTexels);
}

float SampleShadow(vec3 worldPosition, vec3 normal, LightData light, uint lightIndex)
{
    if (uShadow.params.x < 0.5 || uint(uShadow.params.z + 0.5) != lightIndex || (inObjectFlags & PHYSARA_OBJECT_RECEIVE_SHADOW) == 0u)
    {
        return 1.0;
    }

    vec4 lightClip = uShadow.lightViewProjection * vec4(worldPosition, 1.0);
    vec3 projected = lightClip.xyz / max(lightClip.w, PHYSARA_EPSILON);
    vec3 shadowCoord = projected * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return 1.0;
    }

    vec3 lightToSurface = SafeNormalize(light.directionType.xyz);
    float NoL = Saturate(dot(normal, -lightToSurface));
    float receiverBiasScale = max(uShadow.controls.x, 0.0);
    float bias = max(light.shadowParams.y * receiverBiasScale * (1.0 - NoL), light.shadowParams.y * receiverBiasScale * 0.25);
    uint algorithm = uint(uShadow.controls.y + 0.5);
    float filterRadiusTexels = max(uShadow.controls.z, 0.25);
    float lightSizeTexels = max(uShadow.controls.w, 0.25);
    float texel = max(uShadow.params.w, 1.0 / max(uShadow.params.y, 1.0));
    float receiverDepth = clamp(shadowCoord.z - bias, 0.0, 1.0);

    if (algorithm == PHYSARA_SHADOW_HARD)
    {
        return CompareShadowDepth(shadowCoord.xy, receiverDepth);
    }
    if (algorithm == PHYSARA_SHADOW_PCF_5X5)
    {
        return SampleShadowGrid(shadowCoord.xy, receiverDepth, texel, 2, filterRadiusTexels);
    }
    if (algorithm == PHYSARA_SHADOW_POISSON_16)
    {
        return SampleShadowPoisson16(shadowCoord.xy, receiverDepth, texel, filterRadiusTexels);
    }
    if (algorithm == PHYSARA_SHADOW_PCSS)
    {
        return SampleShadowPCSS(shadowCoord.xy, receiverDepth, texel, filterRadiusTexels, lightSizeTexels);
    }
    return SampleShadowGrid(shadowCoord.xy, receiverDepth, texel, 1, filterRadiusTexels);
}

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    
    if (inputs.hasBaseColorTexture)
    {
        vec4 baseColorSample = texture(uBaseColorTexture, SelectTexCoord(inputs.baseColorTexCoord));
        inputs.baseColor *= vec4(SrgbToLinear(baseColorSample.rgb), baseColorSample.a);
    }
    if (inputs.hasMetallicRoughnessTexture)
    {
        vec4 mrSample = texture(uMetallicRoughnessTexture, SelectTexCoord(inputs.metallicRoughnessTexCoord));
        inputs.perceptualRoughness *= mrSample.g;
        inputs.metallic *= mrSample.b;
    }
    if (inputs.hasOcclusionTexture)
    {
        float occlusionSample = texture(uOcclusionTexture, SelectTexCoord(inputs.occlusionTexCoord)).r;
        inputs.ambientOcclusion *= occlusionSample;
    }
    if (inputs.hasEmissiveTexture)
    {
        vec3 emissiveSample = texture(uEmissiveTexture, SelectTexCoord(inputs.emissiveTexCoord)).rgb;
        inputs.emissiveColor *= SrgbToLinear(emissiveSample);
    }
    
    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }
    
    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        outColor = vec4(material.emissive, material.baseColor.a);
        return;
    }
    
    ShadingContext context;
    context.worldPosition = inWorldPosition;
    vec3 geometricNormal = normalize(inWorldNormal);
    if (inputs.doubleSided && ! gl_FrontFacing)
    {
        geometricNormal = -geometricNormal;
    }
    context.normal = ResolveWorldNormal(inputs, geometricNormal, inWorldTangent, SelectTexCoord(inputs.normalTexCoord));
    context.view = normalize(GetCameraPosition(uCamera) - inWorldPosition);
    material.energyCompensation = ComputeIBLEnergyCompensation(material, context.normal, context.view);

    uint debugView = uint(uDebugParams.x + 0.5);
    if (debugView == 1u)
    {
        outColor = vec4(context.normal * 0.5 + 0.5, material.baseColor.a);
        return;
    }
    
    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
    for(uint i = 0u; i < lightCount; ++ i)
    {
        float shadowVisibility = SampleShadow(context.worldPosition, context.normal, uLights[i], i);
        color += EvaluateLight(material, context, uLights[i]) * shadowVisibility;
    }
    color += EvaluateIBL(material, context.normal, context.view);
    color += material.emissive;
    outColor = vec4(color, material.baseColor.a);
}