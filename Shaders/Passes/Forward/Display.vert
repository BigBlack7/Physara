#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>

// 全屏三角形覆盖整个画布, 包括工作颜色中已初始化的相机留边.
void main()
{
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
