#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/FrameUniforms.glsl"

layout(location = 0)out vec3 outNearPoint;
layout(location = 1)out vec3 outFarPoint;

const vec2 kPositions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0));

vec3 UnprojectPoint(vec2 position, float depth)
{
    vec4 world = uFrame.camera.inverseViewProjection * vec4(position, depth, 1.0);
    return world.xyz / world.w;
}

void main()
{
    vec2 position = kPositions[gl_VertexID];
    outNearPoint = UnprojectPoint(position, -1.0);
    outFarPoint = UnprojectPoint(position, 1.0);
    gl_Position = vec4(position, 0.0, 1.0);
}