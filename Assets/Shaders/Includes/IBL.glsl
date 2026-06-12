#ifndef PHYSARA_IBL_GLSL
#define PHYSARA_IBL_GLSL

#include "FrameUniforms.glsl"
#include "Material.glsl"

layout(binding = PHYSARA_BINDING_IBL_PREFILTERED_TEXTURE)uniform samplerCube uIBLPrefilteredTexture;
layout(binding = PHYSARA_BINDING_IBL_BRDF_LUT)uniform sampler2D uIBLBRDFLUT;

bool HasIBL()
{
    return uFrame.ibl.params.z > 0.5;
}

vec2 SampleIBLDFG(float NoV, float perceptualRoughness)
{
    return textureLod(uIBLBRDFLUT, vec2(NoV, perceptualRoughness), 0.0).rg;
}

vec3 ComputeIBLEnergyCompensation(PixelMaterial material, vec3 normal, vec3 view)
{
    if (!HasIBL())
    {
        return vec3(1.0);
    }

    float NoV = max(dot(normal, view), PHYSARA_EPSILON);
    return EnergyCompensation(material.f0, SampleIBLDFG(NoV, material.perceptualRoughness));
}

vec3 EvaluateIrradianceSH(vec3 normal)
{
    vec3 n = normalize(normal);
    vec3 irradiance =
        uFrame.ibl.irradianceSH[0].rgb * 0.282095 +
        uFrame.ibl.irradianceSH[1].rgb * (0.488603 * n.y) +
        uFrame.ibl.irradianceSH[2].rgb * (0.488603 * n.z) +
        uFrame.ibl.irradianceSH[3].rgb * (0.488603 * n.x) +
        uFrame.ibl.irradianceSH[4].rgb * (1.092548 * n.x * n.y) +
        uFrame.ibl.irradianceSH[5].rgb * (1.092548 * n.y * n.z) +
        uFrame.ibl.irradianceSH[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) +
        uFrame.ibl.irradianceSH[7].rgb * (1.092548 * n.x * n.z) +
        uFrame.ibl.irradianceSH[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(irradiance, vec3(0.0)) * uFrame.ibl.params.x;
}

vec3 EvaluateIBL(PixelMaterial material, vec3 normal, vec3 view)
{
    if (!HasIBL())
    {
        return material.diffuseColor * material.ambientOcclusion * 0.04;
    }

    float NoV = max(dot(normal, view), PHYSARA_EPSILON);
    vec3 reflection = reflect(-view, normal);
    float maxLod = max(uFrame.ibl.params.y, 0.0);
    float lod = clamp(material.perceptualRoughness * maxLod, 0.0, maxLod);
    float lod0 = floor(lod);
    float lod1 = min(lod0 + 1.0, maxLod);
    float lodWeight = lod - lod0;

    vec3 irradiance = EvaluateIrradianceSH(normal);
    vec3 diffuse = material.diffuseColor * irradiance * PHYSARA_INV_PI;
    diffuse *= material.ambientOcclusion;

    vec3 prefiltered0 = textureLod(uIBLPrefilteredTexture, reflection, lod0).rgb;
    vec3 prefiltered1 = textureLod(uIBLPrefilteredTexture, reflection, lod1).rgb;
    vec3 prefiltered = mix(prefiltered0, prefiltered1, lodWeight) * uFrame.ibl.params.x;
    vec2 dfg = SampleIBLDFG(NoV, material.perceptualRoughness);
    vec3 specularColor = mix(dfg.xxx, dfg.yyy, material.f0);
    vec3 specular = prefiltered * specularColor * material.energyCompensation;
    specular *= ComputeSpecularAO(NoV, material.ambientOcclusion, material.roughness);

    return diffuse + specular;
}

#endif