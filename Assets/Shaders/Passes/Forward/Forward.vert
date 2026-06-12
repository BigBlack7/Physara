#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/FrameUniforms.glsl"

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec4 inTangent;
layout(location = 3)in vec2 inTexCoord0;
layout(location = 4)in vec2 inTexCoord1;

layout(std430, binding = PHYSARA_BINDING_OBJECTS)readonly buffer ObjectBuffer
{
    ObjectData uObjects[];
};

layout(std430, binding = PHYSARA_BINDING_INSTANCE_INDICES)readonly buffer InstanceIndexBuffer
{
    uint uInstanceObjectIndices[];
};

layout(location = 0)out vec3 outWorldPosition;
layout(location = 1)out vec3 outWorldNormal;
layout(location = 2)out vec4 outWorldTangent;
layout(location = 3)out vec2 outTexCoord0;
layout(location = 4)out vec2 outTexCoord1;
layout(location = 5)flat out uint outMaterialIndex;
layout(location = 6)flat out uint outObjectFlags;

void main()
{
    uint instanceIndex = uint(gl_BaseInstance) + uint(gl_InstanceID);
    uint objectIndex = uInstanceObjectIndices[instanceIndex];
    ObjectData objectData = uObjects[objectIndex];
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    
    outWorldPosition = worldPosition.xyz;
    outWorldNormal = normalize((objectData.inverseTransposeModel * vec4(inNormal, 0.0)).xyz);
    outWorldTangent = vec4(normalize((objectData.model * vec4(inTangent.xyz, 0.0)).xyz), inTangent.w);
    outTexCoord0 = inTexCoord0;
    outTexCoord1 = inTexCoord1;
    outMaterialIndex = objectData.indicesAndFlags.z;
    outObjectFlags = objectData.indicesAndFlags.w;
    
    gl_Position = uFrame.camera.viewProjection * worldPosition;
}