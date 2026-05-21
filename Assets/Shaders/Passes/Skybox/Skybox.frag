#version 460 core

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

layout(binding = PHYSARA_BINDING_SKYBOX_TEXTURE)uniform sampler2D uSkyboxTexture;

layout(location = 0)out vec4 outColor;

float SanitizeHDRComponent(float value)
{
    if (isnan(value))
    {
        return 0.0;
    }
    if (isinf(value))
    {
        return value > 0.0 ? 60000.0 : 0.0;
    }
    return clamp(value, 0.0, 60000.0);
}

vec3 SanitizeHDR(vec3 value)
{
    return vec3(
        SanitizeHDRComponent(value.r),
        SanitizeHDRComponent(value.g),
        SanitizeHDRComponent(value.b));
}

void main()
{
    vec3 direction = normalize(inDirection);
    float longitude = atan(direction.z, direction.x);
    float latitude = acos(clamp(direction.y, -1.0, 1.0));
    vec2 uv = vec2(longitude / (2.0 * PHYSARA_PI) + 0.5, latitude / PHYSARA_PI);
    vec3 hdrColor = SanitizeHDR(texture(uSkyboxTexture, uv).rgb);
    float exposure = max(ExposureFromEV100(GetEV100(uCamera)), PHYSARA_EPSILON);
    float exposureCompensation = exp2(uSkyboxParams.x);
    vec3 sceneColor = clamp(hdrColor * exposureCompensation / exposure, vec3(0.0), vec3(60000.0));
    outColor = vec4(sceneColor, 1.0);
}
