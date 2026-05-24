#ifndef PHYSARA_IBL_GLSL
#define PHYSARA_IBL_GLSL

#include "Material.glsl"

layout(std140, binding = PHYSARA_BINDING_IBL)uniform IBLBuffer
{
    vec4 uIrradianceSH[9];
    vec4 uIBLParams;
};

layout(binding = PHYSARA_BINDING_IBL_PREFILTERED_TEXTURE)uniform samplerCube uIBLPrefilteredTexture;
layout(binding = PHYSARA_BINDING_IBL_BRDF_LUT)uniform sampler2D uIBLBRDFLUT;

bool HasIBL()
{
    return uIBLParams.z > 0.5;
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
        uIrradianceSH[0].rgb * 0.282095 +
        uIrradianceSH[1].rgb * (0.488603 * n.y) +
        uIrradianceSH[2].rgb * (0.488603 * n.z) +
        uIrradianceSH[3].rgb * (0.488603 * n.x) +
        uIrradianceSH[4].rgb * (1.092548 * n.x * n.y) +
        uIrradianceSH[5].rgb * (1.092548 * n.y * n.z) +
        uIrradianceSH[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) +
        uIrradianceSH[7].rgb * (1.092548 * n.x * n.z) +
        uIrradianceSH[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(irradiance, vec3(0.0)) * uIBLParams.x;
}

vec3 EvaluateIBL(PixelMaterial material, vec3 normal, vec3 view)
{
    if (!HasIBL())
    {
        return material.diffuseColor * material.ambientOcclusion * 0.04;
    }

    float NoV = max(dot(normal, view), PHYSARA_EPSILON);
    vec3 reflection = reflect(-view, normal);
    float lod = material.perceptualRoughness * max(uIBLParams.y, 0.0);

    vec3 irradiance = EvaluateIrradianceSH(normal);
    vec3 diffuse = material.diffuseColor * irradiance * PHYSARA_INV_PI;
    diffuse *= material.ambientOcclusion;

    vec3 prefiltered = textureLod(uIBLPrefilteredTexture, reflection, lod).rgb * uIBLParams.x;
    vec2 dfg = SampleIBLDFG(NoV, material.perceptualRoughness);
    vec3 specularColor = material.f0 * (dfg.y - dfg.x) + material.f90 * dfg.x;
    vec3 specular = prefiltered * specularColor * material.energyCompensation;
    specular *= ComputeSpecularAO(NoV, material.ambientOcclusion, material.roughness);

    return diffuse + specular;
}

#endif