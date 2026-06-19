#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;
layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(location = 0)out vec4 outColor;

vec3 SampleSafe(vec2 uv)
{
    vec3 value = texture(uSceneColor, uv).rgb;
    bvec3 invalid = bvec3(isnan(value.r) || isinf(value.r), isnan(value.g) || isinf(value.g), isnan(value.b) || isinf(value.b));
    return clamp(mix(value, vec3(0.0), invalid), vec3(0.0), vec3(60000.0));
}

void main()
{
    vec2 texel = 1.0 / max(vec2(textureSize(uSceneColor, 0)), vec2(1.0));
    vec3 color = SampleSafe(inUV) * 0.125;
    color += (SampleSafe(inUV + vec2(-2.0, -2.0) * texel) +
              SampleSafe(inUV + vec2(2.0, -2.0) * texel) +
              SampleSafe(inUV + vec2(-2.0, 2.0) * texel) +
              SampleSafe(inUV + vec2(2.0, 2.0) * texel)) * 0.03125;
    color += (SampleSafe(inUV + vec2(-2.0, 0.0) * texel) +
              SampleSafe(inUV + vec2(2.0, 0.0) * texel) +
              SampleSafe(inUV + vec2(0.0, -2.0) * texel) +
              SampleSafe(inUV + vec2(0.0, 2.0) * texel)) * 0.0625;
    color += (SampleSafe(inUV + vec2(-1.0, -1.0) * texel) +
              SampleSafe(inUV + vec2(1.0, -1.0) * texel) +
              SampleSafe(inUV + vec2(-1.0, 1.0) * texel) +
              SampleSafe(inUV + vec2(1.0, 1.0) * texel)) * 0.125;
    outColor = vec4(color, 1.0);
}
