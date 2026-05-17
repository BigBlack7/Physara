#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Common.glsl"

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(location = 0)out vec3 outDirection;

const vec2 kPositions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0));

void main()
{
    vec2 position = kPositions[gl_VertexID];
    vec4 viewPosition = uCamera.inverseProjection * vec4(position, 1.0, 1.0);
    vec3 viewDirection = normalize(viewPosition.xyz / viewPosition.w);
    outDirection = normalize((uCamera.inverseView * vec4(viewDirection, 0.0)).xyz);
    gl_Position = vec4(position.xy, 1.0, 1.0);
}