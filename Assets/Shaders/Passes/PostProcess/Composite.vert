#version 460 core
#extension GL_ARB_shading_language_include : require

layout(location = 0)out vec2 outUV;

const vec2 kPositions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0));

void main()
{
    vec2 position = kPositions[gl_VertexID];
    outUV = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}