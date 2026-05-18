#ifndef PHYSARA_COMMON_GLSL
#define PHYSARA_COMMON_GLSL

#define PHYSARA_PI 3.14159265358979323846
#define PHYSARA_INV_PI 0.31830988618379067154
#define PHYSARA_HALF_PI 1.57079632679489661923
#define PHYSARA_EPSILON 1e-5

#define PHYSARA_MAX_LIGHTS 128

#define PHYSARA_BINDING_CAMERA 0
#define PHYSARA_BINDING_OBJECTS 1
#define PHYSARA_BINDING_MATERIALS 2
#define PHYSARA_BINDING_LIGHTS 3
#define PHYSARA_BINDING_POST_PROCESS_SETTINGS 4
#define PHYSARA_BINDING_SKYBOX_SETTINGS 4

#define PHYSARA_BINDING_BASE_COLOR_TEXTURE 0
#define PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE 1
#define PHYSARA_BINDING_NORMAL_TEXTURE 2
#define PHYSARA_BINDING_OCCLUSION_TEXTURE 3
#define PHYSARA_BINDING_EMISSIVE_TEXTURE 4
#define PHYSARA_BINDING_SKYBOX_TEXTURE 5
#define PHYSARA_BINDING_SCENE_COLOR_TEXTURE 6

#define PHYSARA_LIGHT_DIRECTIONAL 0u
#define PHYSARA_LIGHT_POINT 1u
#define PHYSARA_LIGHT_SPOT 2u
#define PHYSARA_LIGHT_AREA 3u

#define PHYSARA_SHADING_MODEL_LIT 0u
#define PHYSARA_SHADING_MODEL_UNLIT 1u

#define PHYSARA_ALPHA_OPAQUE 0u
#define PHYSARA_ALPHA_MASK 1u
#define PHYSARA_ALPHA_BLEND 2u

struct CameraData
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 inverseView;
    mat4 inverseProjection;
    mat4 inverseViewProjection;
    vec4 cameraPositionEV100;
    vec4 viewportRect;
    vec4 clipPlanes;
};

struct ObjectData
{
    mat4 model;
    mat4 inverseTransposeModel;
    vec4 boundsCenterRadius;
    uvec4 indicesAndFlags;
};

struct LightData
{
    vec4 positionRange;
    vec4 directionType;
    vec4 colorIntensity;
    vec4 spotAngles;
    vec4 shadowParams;
};

float GetEV100(CameraData camera)
{
    return camera.cameraPositionEV100.w;
}

vec3 GetCameraPosition(CameraData camera)
{
    return camera.cameraPositionEV100.xyz;
}

float ExposureFromEV100(float ev100)
{
    return 1.0 / (pow(2.0, ev100) * 1.2);
}

#endif