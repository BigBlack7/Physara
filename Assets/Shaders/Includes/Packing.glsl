#ifndef PHYSARA_PACKING_GLSL
#define PHYSARA_PACKING_GLSL

#include "Math.glsl"

vec2 PackNormalOctahedron(vec3 normal)
{
    normal /= max(abs(normal.x) + abs(normal.y) + abs(normal.z), PHYSARA_EPSILON);
    vec2 packed = normal.xy;
    if (normal.z < 0.0)
    {
        packed = (1.0 - abs(packed.yx)) * sign(packed.xy);
    }
    return packed * 0.5 + 0.5;
}

vec3 UnpackNormalOctahedron(vec2 packed)
{
    vec2 f = packed * 2.0 - 1.0;
    vec3 normal = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    if (normal.z < 0.0)
    {
        normal.xy = (1.0 - abs(normal.yx)) * sign(normal.xy);
    }
    return SafeNormalize(normal);
}

vec4 EncodeRGBE(vec3 color)
{
    float maxComponent = max(max(color.r, color.g), color.b);
    if (maxComponent < 1e-32)
    {
        return vec4(0.0);
    }

    float exponent = ceil(log2(maxComponent));
    return vec4(color / exp2(exponent), (exponent + 128.0) / 255.0);
}

vec3 DecodeRGBE(vec4 rgbe)
{
    if (rgbe.a <= 0.0)
    {
        return vec3(0.0);
    }
    return rgbe.rgb * exp2(rgbe.a * 255.0 - 128.0);
}

vec4 PackBaseColorAlpha(vec3 baseColor, float alpha)
{
    return vec4(Saturate(baseColor), Saturate(alpha));
}

vec2 PackMetallicRoughness(float metallic, float perceptualRoughness)
{
    return Saturate(vec2(metallic, perceptualRoughness));
}

#endif