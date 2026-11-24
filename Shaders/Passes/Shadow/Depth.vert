#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>
#define PHYSARA_STATIC_SHADOW_DEPTH
#include <Source/Geometry/StaticVertexFetch.glsl>

// 本角色选择当前阴影子视图的常规深度投影. 共享 Depth.frag 继续使用相同的 MASK 覆盖数据.
void main()
{
    FetchStaticVertex();
}
