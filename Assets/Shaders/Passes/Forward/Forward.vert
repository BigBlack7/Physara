#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Common.glsl"

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec4 inTangent;
layout(location = 3)in vec2 inTexCoord0;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std430, binding = PHYSARA_BINDING_OBJECTS)readonly buffer ObjectBuffer
{
    ObjectData uObjects[];
};

layout(location = 0)out vec3 outWorldPosition;
layout(location = 1)out vec3 outWorldNormal;
layout(location = 2)out vec4 outWorldTangent;
layout(location = 3)out vec2 outTexCoord0;
layout(location = 4)flat out uint outMaterialIndex;

void main()
{
    ObjectData objectData = uObjects[gl_BaseInstance + gl_InstanceID];
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    
    outWorldPosition = worldPosition.xyz;
    outWorldNormal = normalize((objectData.inverseTransposeModel * vec4(inNormal, 0.0)).xyz);
    outWorldTangent = vec4(normalize((objectData.model * vec4(inTangent.xyz, 0.0)).xyz), inTangent.w);
    outTexCoord0 = inTexCoord0;
    outMaterialIndex = objectData.indicesAndFlags.z;
    
    gl_Position = uCamera.viewProjection * worldPosition;
}
