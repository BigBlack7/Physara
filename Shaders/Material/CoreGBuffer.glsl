#ifndef PHYSARA_CORE_GBUFFER_GLSL
#define PHYSARA_CORE_GBUFFER_GLSL

#include <Source/Material/CoreSurface.glsl>

// G1 的法线编解码和四附件的材质解释供几何生产者与屏幕消费者共享.
// 调用者从同一像素取得各字段, 从 G3 取得核心资格, 从 D32 取得该表面的深度.
// 上下半球共用有符号八面体坐标, 折叠边界始终给零分量取正号以保留负 Z 极点.
vec2 FoldOctNormal(vec2 xy)
{
    return (1.0 - abs(xy.yx)) * mix(vec2(-1.0), vec2(1.0), greaterThanEqual(xy, vec2(0.0)));
}

// 输入是处理完贴图和背面翻转的单位世界法线. 投影到八面体后, 两个分量直接存入有符号浮点通道.
vec2 EncodeCoreNormal(vec3 normal)
{
    vec2 xy = normal.xy / dot(abs(normal), vec3(1.0));
    return normal.z < 0.0 ? FoldOctNormal(xy) : xy;
}

// pixel 包含与视线无关的波瓣. 保留原基色与金属度供后续消费, 自发光已经应用本帧预曝光.
struct CoreSurfaceSample
{
    PixelParams pixel;
    vec3 shadingNormal;
    vec3 baseColor;
    float metallic;
    float occlusion;
    vec3 preExposedEmission;
};

// 有限远平面通过深度测试时也会写出 D32=0. 核心片元置位, 背景或被扩展表面覆盖的像素清位.
bool IsCoreSurface(uint flags)
{
    return (flags & CoreDeferredEligible) != 0u;
}

// G0.rgb 已由 sRGB 视图还原到线性域. 展开量化后的 oct 坐标并重新归一化,
// 用 G1.z 中已过滤的感知粗糙度还原波瓣, G2 的工作域自发光可直接用于合成.
CoreSurfaceSample DecodeCoreSurface(vec4 g0, vec4 g1, vec3 g2)
{
    vec3 normal = vec3(g1.xy, 1.0 - abs(g1.x) - abs(g1.y));
    if (normal.z < 0.0)
    {
        normal.xy = FoldOctNormal(normal.xy);
    }
    return CoreSurfaceSample(CorePixelParams(g0.rgb, g1.w, g1.z), normalize(normal),
                             g0.rgb, g1.w, g0.a, g2);
}
#endif
