#ifndef PHYSARA_BRDF_GLSL
#define PHYSARA_BRDF_GLSL

#include "Math.glsl"

struct BRDFInputs
{
    vec3 diffuseColor;
    vec3 f0;
    vec3 f90;
    float perceptualRoughness;
    float roughness;
    float NoV;
    float NoL;
    float NoH;
    float LoH;
};

float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / max(1.0 - NoH * NoH + a * a, PHYSARA_EPSILON);
    return k * k * PHYSARA_INV_PI;
}

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a2 = roughness * roughness;
    float GGXV = NoL * sqrt(max(NoV * NoV * (1.0 - a2) + a2, 0.0));
    float GGXL = NoV * sqrt(max(NoL * NoL * (1.0 - a2) + a2, 0.0));
    return 0.5 / max(GGXV + GGXL, PHYSARA_EPSILON);
}

vec3 F_Schlick(float u, vec3 f0)
{
    float f = pow(1.0 - Saturate(u), 5.0);
    return f + f0 * (1.0 - f);
}

float F_Schlick(float u, float f0, float f90)
{
    return f0 + (f90 - f0) * pow(1.0 - Saturate(u), 5.0);
}

float Fd_Lambert()
{
    return PHYSARA_INV_PI;
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness)
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * PHYSARA_INV_PI;
}

vec3 EnergyCompensation(vec3 f0, vec2 dfg)
{
    return 1.0 + f0 * (1.0 / max(dfg.y, PHYSARA_EPSILON) - 1.0);
}

BRDFInputs BuildBRDFInputs(vec3 normal, vec3 view, vec3 light, vec3 diffuseColor, vec3 f0, float perceptualRoughness)
{
    vec3 halfVector = SafeNormalize(view + light);

    BRDFInputs inputs;
    inputs.diffuseColor = diffuseColor;
    inputs.f0 = f0;
    inputs.f90 = vec3(Saturate(Luminance(f0) * 50.0));
    inputs.perceptualRoughness = clamp(perceptualRoughness, 0.045, 1.0);
    inputs.roughness = inputs.perceptualRoughness * inputs.perceptualRoughness;
    inputs.NoV = abs(dot(normal, view)) + PHYSARA_EPSILON;
    inputs.NoL = Saturate(dot(normal, light));
    inputs.NoH = Saturate(dot(normal, halfVector));
    inputs.LoH = Saturate(dot(light, halfVector));
    return inputs;
}

vec3 EvaluateSpecularBRDF(BRDFInputs inputs)
{
    float D = D_GGX(inputs.NoH, inputs.roughness);
    float V = V_SmithGGXCorrelated(inputs.NoV, inputs.NoL, inputs.roughness);
    vec3 F = F_Schlick(inputs.LoH, inputs.f0);
    return (D * V) * F;
}

vec3 EvaluateDiffuseBRDF(BRDFInputs inputs)
{
    return inputs.diffuseColor * Fd_Burley(inputs.NoV, inputs.NoL, inputs.LoH, inputs.roughness);
}

vec3 EvaluateSurfaceBRDF(BRDFInputs inputs)
{
    return EvaluateDiffuseBRDF(inputs) + EvaluateSpecularBRDF(inputs);
}

#endif