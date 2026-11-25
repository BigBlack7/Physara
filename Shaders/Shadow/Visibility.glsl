#ifndef PHYSARA_SHADOW_VISIBILITY_GLSL
#define PHYSARA_SHADOW_VISIBILITY_GLSL

#include <Source/Geometry/LightingPosition.glsl>

// 同时服务逐灯光照和诊断. subview 是组内级联或面号, -1 表示覆盖外, nextWeight 仅用于级联过渡.
struct ShadowResult
{
    int subview;
    float nextWeight;
    float visibility;
};

const ShadowResult OutsideShadow = ShadowResult(-1, 0.0, 1.0);

// 朝光侧的世界几何法线. 掠射时接近给定的世界 texel 偏移上限.
vec3 ShadowReceiverOffset(vec3 geometricNormal, vec3 toLight, float maxOffset)
{
    float cosine = clamp(dot(geometricNormal, toLight), -1.0, 1.0);
    float signToLight = cosine < 0.0 ? -1.0 : 1.0;
    return geometricNormal * (maxOffset * signToLight * (1.0 - abs(cosine)));
}

// 按常规阴影深度比较每个有效样点. 过滤的量是比较结果, 等深可见.
float CompareShadowDepth(ivec2 texel, uint layer, float depth)
{
    return depth <= texelFetch(shadowImage, ivec3(texel, int(layer)), 0).r ? 1.0 : 0.0;
}

// 最大核完整留在给定物理层内. 失效位置在 fetch 前返回中性.
float FilterShadow(vec3 ndc, uint layer, out bool valid)
{
    vec2 uv = ndc.xy * vec2(0.5, -0.5) + 0.5;
    valid = layer < shadowView.layerCount &&
        all(greaterThanEqual(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0))) &&
        ndc.z >= 0.0 && ndc.z <= 1.0;
    if (!valid)
    {
        return 1.0;
    }
    // UV 位于半开区间 [0,1). 两档共用同一中心 texel, PCF 还须容纳八个邻点.
    ivec2 center = ivec2(uv * float(shadowView.edge));
    int radius = shadowView.pcf != 0u ? 1 : 0;
    valid = all(greaterThanEqual(center - radius, ivec2(0))) &&
        all(lessThan(center + radius, ivec2(int(shadowView.edge))));
    if (!valid)
    {
        return 1.0;
    }
    if (radius == 0)
    {
        return CompareShadowDepth(center, layer, ndc.z);
    }
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            visible += CompareShadowDepth(center + ivec2(x, y), layer, ndc.z);
        }
    }
    return visible * (1.0 / 9.0);
}

#include <Source/Shadow/Directional.glsl>
#include <Source/Shadow/Local.glsl>

// 组首行由本轮灯表或诊断选定. 0 是无阴影引用, 后续层以组内下标寻址.
ShadowResult QueryShadow(uint reference, LightingSurface surface, vec3 geometricNormal)
{
    if (reference == 0u || reference > shadowView.layerCount)
    {
        return OutsideShadow;
    }
    uint base = reference - 1u;
    ForwardShadowLayer group = shadowLayers.records[base];
    if (group.count == 0u || group.count > shadowView.layerCount - base)
    {
        return OutsideShadow;
    }
    // kind 0/1/2 与 CPU 的 Directional/Point/Spot 同序.
    return group.kind == 0u ? QueryDirectionalShadow(base, group, surface, geometricNormal) :
                             QueryLocalShadow(base, group, surface, geometricNormal);
}

#endif
