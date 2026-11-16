#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>
#include <Source/Geometry/StaticVertexFetch.glsl>

// 两个几何 Pass 使用同一生成接口与位置入口, 不在阶段入口重复实现变换或坐标适配.
void main()
{
    FetchStaticVertex();
}
