#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/FrameUniforms.glsl"

layout(location = 0)out vec3 outDirection;

const uint kSegments = 64u;
const uint kRings = 32u;

vec2 SphereUVFromVertexID(uint vertexID)
{
    uint quad = vertexID / 6u;
    uint vertex = vertexID - quad * 6u;
    uint x = quad % kSegments;
    uint y = quad / kSegments;

    uint localX = 0u;
    uint localY = 0u;
    if (vertex == 1u || vertex == 2u || vertex == 4u)
    {
        localX = 1u;
    }
    if (vertex == 2u || vertex == 3u || vertex == 4u)
    {
        localY = 1u;
    }

    return vec2(float(x + localX) / float(kSegments), float(y + localY) / float(kRings));
}

vec3 DirectionFromSphereUV(vec2 uv)
{
    float phi = uv.x * 2.0 * PHYSARA_PI;
    float theta = uv.y * PHYSARA_PI;
    float sinTheta = sin(theta);
    return vec3(cos(phi) * sinTheta, cos(theta), sin(phi) * sinTheta);
}

void main()
{
    vec3 localDirection = DirectionFromSphereUV(SphereUVFromVertexID(uint(gl_VertexID)));
    float radius = max(uFrame.camera.clipPlanes.y * 0.5, 1000.0);
    mat4 viewWithoutTranslation = mat4(mat3(uFrame.camera.view));
    vec4 clipPosition = uFrame.camera.projection * viewWithoutTranslation * vec4(localDirection * radius, 1.0);
    outDirection = normalize(localDirection);
    gl_Position = clipPosition.xyww;
}