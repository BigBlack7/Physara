#ifndef PHYSARA_SURFACE_GEOMETRY_GLSL
#define PHYSARA_SURFACE_GEOMETRY_GLSL

// 世界空间法线. geometricNormal 用于接收阴影偏移, 零表示缺少可用的局部几何估计.
// shadingNormal 是处理完贴图和面向的单位法线, 用于 BRDF.
struct SurfaceGeometry
{
    vec3 geometricNormal;
    vec3 shadingNormal;
};

#endif
