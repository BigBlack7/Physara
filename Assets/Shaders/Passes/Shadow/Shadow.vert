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

void main()
{
    ObjectData objectData = uObjects[gl_BaseInstance + gl_InstanceID];
    gl_Position = uCamera.viewProjection * objectData.model * vec4(inPosition, 1.0);
}