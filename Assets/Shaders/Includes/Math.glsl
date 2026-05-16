#ifndef PHYSARA_MATH_GLSL
#define PHYSARA_MATH_GLSL

#include "Common.glsl"

float Saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 Saturate(vec2 value)
{
    return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 Saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 Saturate(vec4 value)
{
    return clamp(value, vec4(0.0), vec4(1.0));
}

float Square(float value)
{
    return value * value;
}

vec2 Square(vec2 value)
{
    return value * value;
}

vec3 Square(vec3 value)
{
    return value * value;
}

float Remap(float value, float inMin, float inMax, float outMin, float outMax)
{
    float t = (value - inMin) / max(inMax - inMin, PHYSARA_EPSILON);
    return mix(outMin, outMax, t);
}

float Luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 SafeNormalize(vec3 value)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.0 ? value * inversesqrt(lengthSq) : vec3(0.0, 0.0, 1.0);
}

vec3 Orthonormalize(vec3 tangent, vec3 normal)
{
    return SafeNormalize(tangent - normal * dot(normal, tangent));
}

mat3 BuildTBN(vec3 normal, vec4 tangent)
{
    vec3 n = SafeNormalize(normal);
    vec3 t = Orthonormalize(tangent.xyz, n);
    vec3 b = cross(n, t) * tangent.w;
    return mat3(t, b, n);
}

vec3 ApplyNormalMap(vec3 geometryNormal, vec4 tangent, vec3 normalSample, float normalScale)
{
    vec3 mappedNormal = normalSample * 2.0 - 1.0;
    mappedNormal.xy *= normalScale;
    mappedNormal = SafeNormalize(mappedNormal);
    return SafeNormalize(BuildTBN(geometryNormal, tangent) * mappedNormal);
}

float ComputeSpecularAO(float NoV, float ao, float roughness)
{
    return Saturate(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao);
}

float HorizonOcclusion(vec3 reflection, vec3 normal)
{
    float horizon = min(1.0 + dot(reflection, normal), 1.0);
    return horizon * horizon;
}

#endif