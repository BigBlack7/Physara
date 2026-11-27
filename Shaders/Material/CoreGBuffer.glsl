#ifndef PHYSARA_CORE_GBUFFER_GLSL
#define PHYSARA_CORE_GBUFFER_GLSL

#include <Source/Material/CoreSurface.glsl>
#include <Source/Material/OctNormal.glsl>

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
    return CoreSurfaceSample(CorePixelParams(g0.rgb, g1.w, g1.z), DecodeCoreNormal(g1.xy),
                             g0.rgb, g1.w, g0.a, g2);
}
#endif
