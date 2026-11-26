#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>
#include <Source/Geometry/StaticVertexFetch.glsl>

// 本角色接收当前阴影子视图含 caster 偏移的常规深度投影. 共享 Depth.frag 使用相同的 MASK 覆盖数据.
void main()
{
    FetchStaticVertex();
}
