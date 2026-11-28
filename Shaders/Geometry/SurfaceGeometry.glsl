#ifndef PHYSARA_SURFACE_GEOMETRY_GLSL
#define PHYSARA_SURFACE_GEOMETRY_GLSL

// 世界空间法线. geometricNormal 用于接收阴影偏移, 零表示缺少可用的局部几何估计.
// shadingNormal 是处理完贴图和面向的单位法线, 用于 BRDF.
struct SurfaceGeometry
{
    vec3 geometricNormal;
    vec3 shadingNormal;
#ifdef PHYSARA_CLEARCOAT
    vec3 coatNormal;
    vec3 facingGeometricNormal; // 为上层光照统一半球, 原 geometricNormal 仍供阴影查询.
#endif
};

#ifdef PHYSARA_CLEARCOAT
// 带面向的几何法线与贴图后的清漆法线都须朝向眼点, 逐灯和环境镜面共用此条件.
bool CoatFacesView(SurfaceGeometry geometry, vec3 viewDirection)
{
    return dot(geometry.facingGeometricNormal, viewDirection) > 0.0 &&
           dot(geometry.coatNormal, viewDirection) > 0.0;
}
#endif

#endif
