#version 460 core
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif
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

#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
struct MaterialTextureIndices
{
    uvec4 slots0;
    uvec4 slots1;
};

struct BindlessTextureHandle
{
    uvec4 words;
};

layout(std430, binding = PHYSARA_BINDING_MATERIAL_TEXTURE_INDICES)readonly buffer MaterialTextureIndexBuffer
{
    MaterialTextureIndices uMaterialTextureIndices[];
};

layout(std430, binding = PHYSARA_BINDING_BINDLESS_TEXTURE_HANDLES)readonly buffer BindlessTextureHandleBuffer
{
    BindlessTextureHandle uBindlessTextureHandles[];
};
#else
layout(binding = PHYSARA_BINDING_BASE_COLOR_TEXTURE)uniform sampler2D uBaseColorTexture;
layout(binding = PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE)uniform sampler2D uMetallicRoughnessTexture;
layout(binding = PHYSARA_BINDING_NORMAL_TEXTURE)uniform sampler2D uNormalTexture;
layout(binding = PHYSARA_BINDING_OCCLUSION_TEXTURE)uniform sampler2D uOcclusionTexture;
layout(binding = PHYSARA_BINDING_EMISSIVE_TEXTURE)uniform sampler2D uEmissiveTexture;
#endif
layout(binding = PHYSARA_BINDING_SHADOW_MAP)uniform sampler2DArray uShadowMap;

layout(location = 0)out vec4 outColor;

#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
uvec2 GetBindlessTextureHandle(uint slot)
{
    MaterialTextureIndices indices = uMaterialTextureIndices[inMaterialIndex];
    uint textureIndex = slot < 4u ? indices.slots0[int(slot)] : indices.slots1[int(slot - 4u)];
    return uBindlessTextureHandles[textureIndex].words.xy;
}
#endif

vec4 SampleBaseColorTexture(vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(0u)), texCoord);
#else
    return texture(uBaseColorTexture, texCoord);
#endif
}

vec4 SampleMetallicRoughnessTexture(vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(1u)), texCoord);
#else
    return texture(uMetallicRoughnessTexture, texCoord);
#endif
}

vec3 SampleNormalTexture(vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(2u)), texCoord).xyz;
#else
    return texture(uNormalTexture, texCoord).xyz;
#endif
}

float SampleOcclusionTexture(vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(3u)), texCoord).r;
#else
    return texture(uOcclusionTexture, texCoord).r;
#endif
}

vec3 SampleEmissiveTexture(vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(4u)), texCoord).rgb;
#else
    return texture(uEmissiveTexture, texCoord).rgb;
#endif
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
    vec3 tangentNormal = SampleNormalTexture(texCoord) * 2.0 - 1.0;
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
        visibility += CompareShadowDepth(uv + kPoissonDisk16[i] * radius, receiverDepth, cascadeIndex);
    }
    return visibility / 16.0;
}

float FindAverageBlockerDepth(
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
        vec2 sampleUV = uv + kPoissonDisk16[i] * radius;
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
    float blockerDepth = FindAverageBlockerDepth(
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
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
    {
        return false;
    }
    return true;
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

float SampleShadow(vec3 worldPosition, vec3 normal, LightData light, uint lightIndex)
{
    if (uFrame.shadow.params.x < 0.5 || uint(uFrame.shadow.params.z + 0.5) != lightIndex || (inObjectFlags & PHYSARA_OBJECT_RECEIVE_SHADOW) == 0u)
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
            float nextVisibility = SampleCascadeShadow(worldPosition, normal, light, cascadeIndex + 1);
            visibility = mix(visibility, nextVisibility, transition);
        }
    }
    return visibility;
}

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    
    if (inputs.hasBaseColorTexture)
    {
        vec4 baseColorSample = SampleBaseColorTexture(SelectTexCoord(inputs.baseColorTexCoord));
        inputs.baseColor *= baseColorSample;
    }
    if (inputs.hasMetallicRoughnessTexture)
    {
        vec4 mrSample = SampleMetallicRoughnessTexture(SelectTexCoord(inputs.metallicRoughnessTexCoord));
        inputs.perceptualRoughness = mix(inputs.perceptualRoughness, mrSample.g, Saturate(inputs.roughnessTextureInfluence));
        inputs.metallic = mix(inputs.metallic, mrSample.b, Saturate(inputs.metallicTextureInfluence));
    }
    if (inputs.hasOcclusionTexture)
    {
        float occlusionSample = SampleOcclusionTexture(SelectTexCoord(inputs.occlusionTexCoord));
        inputs.ambientOcclusion = mix(inputs.ambientOcclusion, occlusionSample, Saturate(inputs.ambientOcclusionTextureInfluence));
    }
    if (inputs.hasEmissiveTexture)
    {
        vec3 emissiveSample = SampleEmissiveTexture(SelectTexCoord(inputs.emissiveTexCoord));
        inputs.emissiveColor *= emissiveSample;
    }
    
    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }
    
    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        vec3 unlitRadiance = material.baseColor.rgb + material.emissive;
        outColor = vec4(unlitRadiance * GetPreExposure(uFrame.camera), material.baseColor.a);
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
    context.view = normalize(GetCameraPosition(uFrame.camera) - inWorldPosition);
    material.energyCompensation = ComputeIBLEnergyCompensation(material, context.normal, context.view);

    uint debugView = uint(uFrame.debugParams.x + 0.5);
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
    color += material.emissive * GetPreExposure(uFrame.camera);
    outColor = vec4(color, material.baseColor.a);
}
