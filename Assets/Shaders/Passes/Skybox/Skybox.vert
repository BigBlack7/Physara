#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Common.glsl"

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(location = 0)out vec3 outDirection;

const vec3 kPositions[36] = vec3[36](
    vec3(-1.0, -1.0, 1.0), vec3(1.0, -1.0, 1.0), vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0), vec3(-1.0, 1.0, 1.0), vec3(-1.0, -1.0, 1.0),

    vec3(1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3(-1.0, 1.0, -1.0),
    vec3(-1.0, 1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0),

    vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, 1.0, 1.0),
    vec3(-1.0, 1.0, 1.0), vec3(-1.0, 1.0, -1.0), vec3(-1.0, -1.0, -1.0),

    vec3(1.0, -1.0, 1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0),
    vec3(1.0, 1.0, -1.0), vec3(1.0, 1.0, 1.0), vec3(1.0, -1.0, 1.0),

    vec3(-1.0, 1.0, 1.0), vec3(1.0, 1.0, 1.0), vec3(1.0, 1.0, -1.0),
    vec3(1.0, 1.0, -1.0), vec3(-1.0, 1.0, -1.0), vec3(-1.0, 1.0, 1.0),

    vec3(-1.0, -1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, -1.0, 1.0),
    vec3(1.0, -1.0, 1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, -1.0, -1.0));

void main()
{
    vec3 position = kPositions[gl_VertexID];
    mat4 viewWithoutTranslation = mat4(mat3(uCamera.view));
    vec4 clipPosition = uCamera.projection * viewWithoutTranslation * vec4(position, 1.0);
    outDirection = position;
    gl_Position = clipPosition.xyww;
}