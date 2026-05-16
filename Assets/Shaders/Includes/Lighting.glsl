#ifndef PHYSARA_LIGHTING_GLSL
#define PHYSARA_LIGHTING_GLSL

#include "Material.glsl"

struct ShadingContext
{
    vec3 worldPosition;
    vec3 normal;
    vec3 view;
};

float GetSquareFalloffAttenuation(vec3 positionToLight, float inverseRange)
{
    float distanceSquare = dot(positionToLight, positionToLight);
    float factor = distanceSquare * inverseRange * inverseRange;
    float smoothFactor = max(1.0 - factor * factor, 0.0);
    return (smoothFactor * smoothFactor) / max(distanceSquare, PHYSARA_EPSILON);
}

float GetSpotAngleAttenuation(vec3 light, vec3 lightDirection, vec2 spotScaleOffset)
{
    float cd = dot(normalize(-lightDirection), light);
    float attenuation = Saturate(cd * spotScaleOffset.x + spotScaleOffset.y);
    return attenuation * attenuation;
}

vec3 EvaluateDirectionalLight(PixelMaterial material, ShadingContext context, LightData light)
{
    vec3 l = SafeNormalize(-light.directionType.xyz);
    BRDFInputs brdf = BuildMaterialBRDFInputs(material, context.normal, context.view, l);
    vec3 illuminance = light.colorIntensity.rgb * light.colorIntensity.a * brdf.NoL;
    return EvaluateSurfaceBRDF(brdf) * illuminance;
}

vec3 EvaluatePunctualLight(PixelMaterial material, ShadingContext context, LightData light)
{
    vec3 positionToLight = light.positionRange.xyz - context.worldPosition;
    vec3 l = SafeNormalize(positionToLight);
    BRDFInputs brdf = BuildMaterialBRDFInputs(material, context.normal, context.view, l);

    float inverseRange = 1.0 / max(light.positionRange.w, PHYSARA_EPSILON);
    float attenuation = GetSquareFalloffAttenuation(positionToLight, inverseRange);
    if (uint(light.directionType.w) == PHYSARA_LIGHT_SPOT)
    {
        attenuation *= GetSpotAngleAttenuation(l, light.directionType.xyz, light.spotAngles.xy);
    }

    vec3 luminousIntensity = light.colorIntensity.rgb * light.colorIntensity.a;
    return EvaluateSurfaceBRDF(brdf) * luminousIntensity * attenuation * brdf.NoL;
}

vec3 EvaluateLight(PixelMaterial material, ShadingContext context, LightData light)
{
    uint lightType = uint(light.directionType.w);
    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        return vec3(0.0);
    }
    if (lightType == PHYSARA_LIGHT_DIRECTIONAL)
    {
        return EvaluateDirectionalLight(material, context, light);
    }
    if (lightType == PHYSARA_LIGHT_POINT || lightType == PHYSARA_LIGHT_SPOT)
    {
        return EvaluatePunctualLight(material, context, light);
    }
    return vec3(0.0);
}

vec3 EvaluateLighting(PixelMaterial material, ShadingContext context, LightData lights[PHYSARA_MAX_LIGHTS], uint lightCount)
{
    vec3 color = vec3(0.0);
    uint count = min(lightCount, uint(PHYSARA_MAX_LIGHTS));
    for (uint i = 0u; i < count; ++i)
    {
        color += EvaluateLight(material, context, lights[i]);
    }
    return color + material.emissive;
}

#endif