#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Common.glsl"

layout(location = 0)in vec3 inDirection;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(binding = PHYSARA_BINDING_SKYBOX_TEXTURE)uniform samplerCube uSkyboxTexture;

layout(location = 0)out vec4 outColor;

vec3 LinearToSrgb(vec3 value)
{
    return pow(max(value, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 TonemapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(uSkyboxTexture, normalize(inDirection)).rgb;
    outColor = vec4(LinearToSrgb(TonemapACES(hdrColor)), 1.0);
}