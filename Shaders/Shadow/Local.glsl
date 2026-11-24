// 在灯相对世界坐标中判定覆盖, 用所选面的无偏透视深度比较. 返回值同时供可见性和诊断着色消费.
#include <Source/Geometry/LightingPosition.glsl>

struct LocalShadowResult
{
    int face; // -1 表示没有合法的比较样点.
    float visibility;
};

#ifdef PHYSARA_POINT_SHADOW_PREVIEW
// 与 CPU 面表 +X,-X,+Y,-Y,+Z,-Z 同序, 等绝对值优先 X、Y、Z.
int PointShadowFace(vec3 delta, vec3 magnitude)
{
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
    {
        return delta.x >= 0.0 ? 0 : 1;
    }
    if (magnitude.y >= magnitude.z)
    {
        return delta.y >= 0.0 ? 2 : 3;
    }
    return delta.z >= 0.0 ? 4 : 5;
}

// face 是组内面序号. 图将同序的紧凑物理层分别绑定为二维视图, 逻辑分配层号只保留在 CPU 计划.
float PointShadowDepth(int face, ivec2 texel)
{
    switch (face)
    {
    case 0: return texelFetch(localShadowImage0, texel, 0).r;
    case 1: return texelFetch(localShadowImage1, texel, 0).r;
    case 2: return texelFetch(localShadowImage2, texel, 0).r;
    case 3: return texelFetch(localShadowImage3, texel, 0).r;
    case 4: return texelFetch(localShadowImage4, texel, 0).r;
    default: return texelFetch(localShadowImage5, texel, 0).r;
    }
}
#endif

// 图只在有效组上执行此诊断. 先核对米制半径和轴深, 再从同一有限透视投影取常规深度和单个 texel.
LocalShadowResult QueryLocalShadow(LightingSurface surface)
{
    const LocalShadowResult outside = LocalShadowResult(-1, 1.0);
    LightingPosition light = LightingPosition(localShadow.originHigh, localShadow.originLow);
    vec3 delta = -LightingDelta(light, surface, 1.0);
    vec3 magnitude = abs(delta);
    float largest = max(magnitude.x, max(magnitude.y, magnitude.z));
    if (!(largest > 0.0 && largest <= localShadow.farPlane))
    {
        return outside;
    }
    // 先按半径归一化, 有限米制范围无需在 float 中平方绝对距离.
    vec3 scaled = delta / localShadow.farPlane;
    if (!(dot(scaled, scaled) <= 1.0))
    {
        return outside;
    }

#ifdef PHYSARA_POINT_SHADOW_PREVIEW
    int face = PointShadowFace(delta, magnitude);
    // 每个 UBO 容纳三个面矩阵. 两块内的排列与深度写入时相同.
    int matrixIndex = face % 3;
    mat4 projection = face < 3 ? localShadow.clipFromLight[matrixIndex] : localShadowMatrices.clipFromLight[matrixIndex];
#else
    int face = 0;
    mat4 projection = localShadow.clipFromLight[0];
#endif
    vec4 clip = projection * vec4(delta, 1.0);
    if (!(clip.w >= localShadow.nearPlane && clip.w <= localShadow.farPlane))
    {
        return outside;
    }
    vec3 ndc = clip.xyz / clip.w;
#ifndef PHYSARA_POINT_SHADOW_PREVIEW
    // 聚光外锥在同一投影中是单位圆. 归一化横向距离可区分窄锥, 其余弦可能已舍入为 1.
    if (!(dot(ndc.xy, ndc.xy) <= 1.0))
    {
        return outside;
    }
#endif
    vec2 uv = ndc.xy * vec2(0.5, -0.5) + 0.5;
    if (!all(greaterThanEqual(uv, vec2(0.0))) || !all(lessThanEqual(uv, vec2(1.0))))
    {
        return outside;
    }
    // 合法的闭边界可落在最后一个 texel. 点光标称面边界在扩展视图内部.
    ivec2 texel = min(ivec2(uv * float(localShadow.edge)), ivec2(int(localShadow.edge) - 1));
#ifdef PHYSARA_POINT_SHADOW_PREVIEW
    float stored = PointShadowDepth(face, texel);
#else
    float stored = texelFetch(localShadowImage0, texel, 0).r;
#endif
    return LocalShadowResult(face, clamp(ndc.z, 0.0, 1.0) <= stored ? 1.0 : 0.0);
}
