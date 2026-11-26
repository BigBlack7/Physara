// 静态片元的世界空间框架与几何镜面过滤入口, 消费 StaticVertexFetch 的插值结果.
// 材质层负责 normal 取样及解码; 本文件只准备方向和光栅像素尺度的方差项.
#include <Source/Common/UnitDirection.glsl>
#include <Source/Geometry/SurfaceGeometry.glsl>

// 未使用材质贴图或面向条件的几何法线, 也供一致控制流中的粗糙度求导使用.
vec3 GeometricNormal()
{
    return UnitOr(vNormal, vec3(0.0, 0.0, 1.0));
}

// 由插值的逆转置法线和仿射切线构造表面. usesNormalMap 为真时 tangentNormal 须已解码及规范化.
// vTangent.w 已包含源切线, 有效 UV 和实例的手性; Pipeline 已将实例镜像计入 gl_FrontFacing.
// 几何法线插值为零时回退世界 +Z, 框架变换结果为零时回退该几何法线, 最后才处理背面.
SurfaceGeometry PrepareSurfaceGeometry(vec3 tangentNormal, bool usesNormalMap)
{
    vec3 geometricNormal = GeometricNormal();
    vec3 shadingNormal = geometricNormal;
    if (usesNormalMap)
    {
        vec3 tangent = UnitOr(vTangent.xyz, vec3(0.0));
        tangent = UnitOr(tangent - geometricNormal * dot(geometricNormal, tangent), vec3(0.0));
        vec3 bitangent = cross(geometricNormal, tangent) * (vTangent.w < 0.0 ? -1.0 : 1.0);
        // 重正交后按已有手性重建副切线. 若贴图方向经整个框架变换后为零, 使用几何法线回退.
        shadingNormal = UnitOr(mat3(tangent, bitangent, geometricNormal) * tangentNormal, geometricNormal);
    }
    // 有效正面已考虑实例绕序. 双面翻转在完成贴图求值后作用于着色法线一次.
    return SurfaceGeometry(geometricNormal, gl_FrontFacing ? shadingNormal : -shadingNormal);
}

// 输入为 PrepareSurfaceGeometry 的单位 geometricNormal, 返回 [0, 0.04] 的 alpha 平方域增量.
// 片元调用方须在 discard 及非一致控制流前取得导数, 再将结果交给 BuildPixelParams 一次.
// 采用固定 Filament 的 variance=0.15 与 threshold=0.2, 阈值先平方; 当前 footprint 为一个光栅像素.
// 过滤模型及近似来源见 Docs/Reference/Filament/Lighting.md 的 11.2 节.
float ComputeGeometricRoughnessKernel(vec3 geometricNormal)
{
    vec3 dx = dFdx(geometricNormal);
    vec3 dy = dFdy(geometricNormal);
    float variance = 0.15 * (dot(dx, dx) + dot(dy, dy));
    return min(2.0 * variance, 0.2 * 0.2);
}
