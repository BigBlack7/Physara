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

// 这里记录已编码输入在本次求和中丢失的有符号余差, 与位移同尺度.
// 相机查格使用余差的绝对值, 逐灯测距按符号补回并合入灯位的上传余量.
struct LightingDeltaTerms
{
    vec3 delta;
    vec3 highResidual;
    vec3 lowResidual;
    vec3 tailResidual;
    vec3 totalResidual;
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
        return LightingDeltaTerms(highDelta, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));
    }
    precise vec3 baseError = SumRemainder(targetHigh, negativeAnchor, base);
    precise vec3 highError = SumRemainder(base, negativeOffset, highDelta);
    precise vec3 error = baseError + highError;
    precise vec3 targetLow = target.low * scale;
    precise vec3 negativeLow = -surface.offset.low * scale;
    precise vec3 low = targetLow + negativeLow;
    precise vec3 tail = error + low;
    precise vec3 delta = highDelta + tail;
    return LightingDeltaTerms(delta, SumRemainder(baseError, highError, error),
                              SumRemainder(targetLow, negativeLow, low), SumRemainder(error, low, tail),
                              SumRemainder(highDelta, tail, delta));
}

vec3 LightingDelta(LightingPosition target, LightingSurface surface, float scale, out vec3 roundoff)
{
    LightingDeltaTerms terms = ComputeLightingDeltaTerms(target, surface, scale);
    // Froxel 的同格判定使用各轴上未进入位移的余差绝对值之和.
    roundoff = abs(terms.highResidual) + abs(terms.lowResidual) + abs(terms.tailResidual) + abs(terms.totalResidual);
    return terms.delta;
}

vec3 LightingDelta(LightingPosition target, LightingSurface surface, float scale)
{
    return ComputeLightingDeltaTerms(target, surface, scale).delta;
}

// 高低两组消去后, 将灯位第三项和本次作差的有符号余差合回小尺度位移. 三项采用同一指数尺度.
vec3 CompensatedLightingDelta(LightingPosition target, vec3 targetResidual, LightingSurface surface, float scale)
{
    LightingDeltaTerms terms = ComputeLightingDeltaTerms(target, surface, scale);
    precise vec3 correction = (terms.highResidual + terms.lowResidual) + (terms.tailResidual + terms.totalResidual);
    precise vec3 remainder = correction + targetResidual * scale;
    precise vec3 compensated = terms.delta + remainder;
    return compensated;
}

#endif
