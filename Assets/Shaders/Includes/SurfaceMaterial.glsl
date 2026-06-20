#ifndef PHYSARA_SURFACE_MATERIAL_GLSL
#define PHYSARA_SURFACE_MATERIAL_GLSL

#include "Material.glsl"

layout(std430, binding = PHYSARA_BINDING_MATERIALS)readonly buffer MaterialBuffer
{
    MaterialData uMaterials[];
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

uvec2 GetBindlessTextureHandle(uint materialIndex, uint slot)
{
    MaterialTextureIndices indices = uMaterialTextureIndices[materialIndex];
    uint textureIndex = slot < 4u ? indices.slots0[int(slot)] : indices.slots1[int(slot - 4u)];
    return uBindlessTextureHandles[textureIndex].words.xy;
}
#else
layout(binding = PHYSARA_BINDING_BASE_COLOR_TEXTURE)uniform sampler2D uBaseColorTexture;
layout(binding = PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE)uniform sampler2D uMetallicRoughnessTexture;
layout(binding = PHYSARA_BINDING_NORMAL_TEXTURE)uniform sampler2D uNormalTexture;
layout(binding = PHYSARA_BINDING_OCCLUSION_TEXTURE)uniform sampler2D uOcclusionTexture;
layout(binding = PHYSARA_BINDING_EMISSIVE_TEXTURE)uniform sampler2D uEmissiveTexture;
#endif

vec4 SampleSurfaceBaseColor(uint materialIndex, vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(materialIndex, 0u)), texCoord);
#else
    return texture(uBaseColorTexture, texCoord);
#endif
}

vec4 SampleSurfaceMetallicRoughness(uint materialIndex, vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(materialIndex, 1u)), texCoord);
#else
    return texture(uMetallicRoughnessTexture, texCoord);
#endif
}

vec3 SampleSurfaceNormal(uint materialIndex, vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(materialIndex, 2u)), texCoord).xyz;
#else
    return texture(uNormalTexture, texCoord).xyz;
#endif
}

float SampleSurfaceOcclusion(uint materialIndex, vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(materialIndex, 3u)), texCoord).r;
#else
    return texture(uOcclusionTexture, texCoord).r;
#endif
}

vec3 SampleSurfaceEmissive(uint materialIndex, vec2 texCoord)
{
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
    return texture(sampler2D(GetBindlessTextureHandle(materialIndex, 4u)), texCoord).rgb;
#else
    return texture(uEmissiveTexture, texCoord).rgb;
#endif
}

vec2 SelectSurfaceTexCoord(uint texCoordSet, vec2 texCoord0, vec2 texCoord1)
{
    return texCoordSet == 1u ? texCoord1 : texCoord0;
}

void ApplySurfaceTextures(
    inout MaterialInputs inputs,
    uint materialIndex,
    vec2 texCoord0,
    vec2 texCoord1)
{
    if (inputs.hasBaseColorTexture)
    {
        inputs.baseColor *= SampleSurfaceBaseColor(
            materialIndex,
            SelectSurfaceTexCoord(inputs.baseColorTexCoord, texCoord0, texCoord1));
    }
    if (inputs.hasMetallicRoughnessTexture)
    {
        vec4 sampleValue = SampleSurfaceMetallicRoughness(
            materialIndex,
            SelectSurfaceTexCoord(inputs.metallicRoughnessTexCoord, texCoord0, texCoord1));
        inputs.perceptualRoughness = mix(
            inputs.perceptualRoughness,
            sampleValue.g,
            Saturate(inputs.roughnessTextureInfluence));
        inputs.metallic = mix(inputs.metallic, sampleValue.b, Saturate(inputs.metallicTextureInfluence));
    }
    if (inputs.hasOcclusionTexture)
    {
        float sampleValue = SampleSurfaceOcclusion(
            materialIndex,
            SelectSurfaceTexCoord(inputs.occlusionTexCoord, texCoord0, texCoord1));
        inputs.ambientOcclusion = mix(
            inputs.ambientOcclusion,
            sampleValue,
            Saturate(inputs.ambientOcclusionTextureInfluence));
    }
    if (inputs.hasEmissiveTexture)
    {
        inputs.emissiveColor *= SampleSurfaceEmissive(
            materialIndex,
            SelectSurfaceTexCoord(inputs.emissiveTexCoord, texCoord0, texCoord1));
    }
}

vec3 ResolveSurfaceWorldNormal(
    MaterialInputs inputs,
    uint materialIndex,
    vec3 geometricNormal,
    vec4 worldTangent,
    vec2 texCoord0,
    vec2 texCoord1)
{
    if (!inputs.hasNormalTexture)
    {
        return normalize(geometricNormal);
    }

    vec3 n = normalize(geometricNormal);
    vec3 t = normalize(worldTangent.xyz - n * dot(n, worldTangent.xyz));
    vec3 b = normalize(cross(n, t) * worldTangent.w);
    vec3 tangentNormal = SampleSurfaceNormal(
        materialIndex,
        SelectSurfaceTexCoord(inputs.normalTexCoord, texCoord0, texCoord1)) * 2.0 - 1.0;
    if (inputs.flipNormalY)
    {
        tangentNormal.y = -tangentNormal.y;
    }
    tangentNormal.xy *= inputs.normalScale;
    return normalize(mat3(t, b, n) * tangentNormal);
}

#endif