#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>
#define PHYSARA_DIRECTIONAL_SHADOW_DEPTH
#include <Source/Geometry/StaticVertexFetch.glsl>

// 本角色选择当前级联的常规深度投影. UV, 顶点色和材质行继续交给共享 Depth.frag 裁去 MASK 孔.
void main()
{
    FetchStaticVertex();
}
