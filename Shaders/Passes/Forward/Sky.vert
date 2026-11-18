#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>

// 覆盖当前有效矩形, 插值的 clip 坐标用于重建该像素的方向.
void main()
{
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vClip = corner * 2.0 - 1.0;
    gl_Position = vec4(vClip, 0.0, 1.0);
}
