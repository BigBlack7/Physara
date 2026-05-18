#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Common.glsl"

layout(location = 0)in vec3 inDirection;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std140, binding = PHYSARA_BINDING_SKYBOX_SETTINGS)uniform SkyboxSettingsBuffer
{
    vec4 uSkyboxParams;
};

layout(binding = PHYSARA_BINDING_SKYBOX_TEXTURE)uniform samplerCube uSkyboxTexture;

layout(location = 0)out vec4 outColor;

void main()
{
    vec3 hdrColor = texture(uSkyboxTexture, normalize(inDirection)).rgb;
    float exposure = max(ExposureFromEV100(GetEV100(uCamera)), PHYSARA_EPSILON);
    float exposureCompensation = exp2(uSkyboxParams.x);
    outColor = vec4(hdrColor * exposureCompensation / exposure, 1.0);
}