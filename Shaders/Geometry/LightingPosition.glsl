#ifndef PHYSARA_LIGHTING_POSITION_GLSL
#define PHYSARA_LIGHTING_POSITION_GLSL

#include <Source/Common/SumRemainder.glsl>

// 顶点写阴影和片元查询在同一世界基点下作差. LightingDelta 的结果指向 target.
// 一个米制向量的高位与余量. 灯和眼点使用世界位置, 表面用同一表示保存相对基点的偏移.
struct LightingPosition
{
    vec3 high;
    vec3 low;
};

// anchor 是同一 Draw 共用的 FP32 世界基点. offset 是相对该原点分开插值的高位和余量.
struct LightingSurface
{
    vec3 anchor;
    LightingPosition offset;
};

// 先计算 (target.high - anchor) - offset.high, 再合入两次减法余量和 target.low - offset.low.
// anchor 与对象行计算 T - A 时使用的 A 相同, 从目标与表面两条路径抵消.
// 所有分量均使用米制坐标, scale 统一调整各项的指数.
vec3 LightingDelta(LightingPosition target, LightingSurface surface, float scale)
{
    vec3 targetHigh = target.high * scale;
    vec3 negativeAnchor = -surface.anchor * scale;
    vec3 negativeOffset = -surface.offset.high * scale;
    precise vec3 base = targetHigh + negativeAnchor;
    precise vec3 highDelta = base + negativeOffset;
    if (any(isinf(highDelta)))
    {
        return highDelta;
    }
    precise vec3 error = SumRemainder(targetHigh, negativeAnchor, base) +
                         SumRemainder(base, negativeOffset, highDelta);
    precise vec3 low = target.low * scale - surface.offset.low * scale;
    precise vec3 delta = highDelta + (error + low);
    return delta;
}

#endif
