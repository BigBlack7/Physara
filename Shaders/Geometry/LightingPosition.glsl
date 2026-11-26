#ifndef PHYSARA_LIGHTING_POSITION_GLSL
#define PHYSARA_LIGHTING_POSITION_GLSL

#include <Source/Common/SumRemainder.glsl>

// 光栅投影和片元查询从同一表面基点作差. LightingDelta 的结果指向 target.
// 米制向量分成高位、低位和编码余量. 灯、眼点与投影原点均可携带三段.
struct LightingPosition
{
    vec3 high;
    vec3 low;
    vec3 residual;
};

// 几何入口以实例基点定位, offset 的高低位分别插值. 屏幕入口保存相对眼点的位移、眼点低位与编码余量.
struct LightingSurface
{
    vec3 anchor;
    LightingPosition offset;
};

// 记录主差值、两侧第三段的差, 以及各次加法的有符号余差. 各项与位移同尺度.
struct LightingDeltaTerms
{
    vec3 delta;
    vec3 highResidual;
    vec3 lowResidual;
    vec3 tailResidual;
    vec3 totalResidual;
    vec3 residualDelta;
    vec3 residualRoundoff;
};

// 先计算 (target.high - anchor) - offset.high, 再合入两次减法余量和 target.low - offset.low.
// anchor 与对象行计算 T - A 时使用的 A 相同, 从目标与表面两条路径抵消.
// 输入为米制坐标, scale 统一调整各项的指数. 位移指向 target.
LightingDeltaTerms ComputeLightingDeltaTerms(LightingPosition target, LightingSurface surface, float scale)
{
    vec3 targetHigh = target.high * scale;
    vec3 negativeAnchor = -surface.anchor * scale;
    vec3 negativeOffset = -surface.offset.high * scale;
    precise vec3 base = targetHigh + negativeAnchor;
    precise vec3 highDelta = base + negativeOffset;
    if (any(isinf(highDelta)))
    {
        return LightingDeltaTerms(highDelta, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));
    }
    precise vec3 baseError = SumRemainder(targetHigh, negativeAnchor, base);
    precise vec3 highError = SumRemainder(base, negativeOffset, highDelta);
    precise vec3 error = baseError + highError;
    precise vec3 targetLow = target.low * scale;
    precise vec3 negativeLow = -surface.offset.low * scale;
    precise vec3 low = targetLow + negativeLow;
    precise vec3 tail = error + low;
    precise vec3 delta = highDelta + tail;
    precise vec3 targetResidual = target.residual * scale;
    precise vec3 negativeResidual = -surface.offset.residual * scale;
    precise vec3 residualDelta = targetResidual + negativeResidual;
    return LightingDeltaTerms(delta, SumRemainder(baseError, highError, error),
                              SumRemainder(targetLow, negativeLow, low), SumRemainder(error, low, tail),
                              SumRemainder(highDelta, tail, delta), residualDelta,
                              SumRemainder(targetResidual, negativeResidual, residualDelta));
}

vec3 LightingDelta(LightingPosition target, LightingSurface surface, float scale, out vec3 roundoff)
{
    LightingDeltaTerms terms = ComputeLightingDeltaTerms(target, surface, scale);
    // Froxel 的同格判定使用各轴上未进入位移的余差绝对值之和.
    roundoff = abs(terms.highResidual) + abs(terms.lowResidual) + abs(terms.tailResidual) + abs(terms.totalResidual) +
               abs(terms.residualDelta) + abs(terms.residualRoundoff);
    return terms.delta;
}

// 光栅、测距和阴影查询将两侧的编码余量与算术余差合回相对位移, 再投影或求长度.
vec3 LightingDelta(LightingPosition target, LightingSurface surface, float scale)
{
    LightingDeltaTerms terms = ComputeLightingDeltaTerms(target, surface, scale);
    precise vec3 correction = (terms.highResidual + terms.lowResidual) + (terms.tailResidual + terms.totalResidual);
    precise vec3 remainder = (correction + terms.residualDelta) + terms.residualRoundoff;
    precise vec3 compensated = terms.delta + remainder;
    return compensated;
}

#endif
