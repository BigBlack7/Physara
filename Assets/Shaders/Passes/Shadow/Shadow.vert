#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Common.glsl"

layout(location = 0)in vec3 inPosition;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std430, binding = PHYSARA_BINDING_OBJECTS)readonly buffer ObjectBuffer
{
    ObjectData uObjects[];
};

layout(std430, binding = PHYSARA_BINDING_INSTANCE_INDICES)readonly buffer InstanceIndexBuffer
{
    uint uInstanceObjectIndices[];
};

void main()
{
    uint instanceIndex = uint(gl_BaseInstance) + uint(gl_InstanceID);
    uint objectIndex = uInstanceObjectIndices[instanceIndex];
    mat4 model = uObjects[objectIndex].model;
    gl_Position = uCamera.viewProjection * model * vec4(inPosition, 1.0);
}