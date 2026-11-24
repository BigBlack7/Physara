// Forward 的逐灯求值. 灯量保留 lux/candela, 距离为米, 输出为当轮预曝光线性工作色.
// 衰减采用项目的四次距离窗和 Khronos 余弦平方角衰减. 逐灯 BRDF 由核心库提供.
#include <Source/BRDF/CoreBrdf.glsl>
#include <Source/Geometry/LightingPosition.glsl>
#include <Source/Lighting/WorkingLight.glsl>

// 距离按最大分量缩放, 避免 r^2 自身的溢出及除以零. D 为发射轴, L 由表面指向灯.
vec3 EvaluateDirect(ForwardLight light, bool directional, PixelParams pixel, vec3 normal, vec3 viewDirection,
                    LightingSurface surface, float exposureMantissa, int exposureExponent, vec3 specularCompensation)
{
    if (light.intensity == 0.0)
    {
        return vec3(0.0);
    }
    vec3 L = -light.direction;
    float attenuation = 1.0;
    float distanceMantissa = 1.0;
    int distanceExponent = 0;
    if (!directional)
    {
        LightingPosition lightPosition = LightingPosition(light.positionHigh, light.positionLow);
        vec3 delta = LightingDelta(lightPosition, surface, 1.0);
        int scaleShift = 0;
        if (any(isinf(delta)) || any(isnan(delta)))
        {
            // 三项各缩小到四分之一后重新相减, 再判断有限范围. 指数补回 2 以恢复米制距离.
            delta = LightingDelta(lightPosition, surface, 0.25);
            scaleShift = 2;
        }
        float scale = max(max(abs(delta.x), abs(delta.y)), abs(delta.z));
        if (!(scale > 0.0))
        {
            return vec3(0.0);
        }
        // r^2 = scale^2 * dot(scaled, scaled). 非零差向量缩放后最大分量绝对值为 1.
        vec3 scaled = delta / scale;
        float lengthSquared = dot(scaled, scaled);
        L = scaled * inversesqrt(lengthSquared);
        if (light.range > 0.0)
        {
            float ratio = (scale / light.range) * float(1 << scaleShift);
            if (!(ratio < 1.0))
            {
                return vec3(0.0);
            }
            float ratioSquared = ratio * ratio * lengthSquared;
            attenuation = max(1.0 - ratioSquared * ratioSquared, 0.0);
        }
        if (light.outerChordSquared > light.innerChordSquared)
        {
            // 点光的弦长端点为零. 两单位方向差的平方等价于 2 * (1 - cos), 保留近轴过渡精度.
            vec3 chord = light.direction + L;
            float angle = clamp((light.outerChordSquared - dot(chord, chord)) /
                                (light.outerChordSquared - light.innerChordSquared), 0.0, 1.0);
            attenuation *= angle * angle;
        }
        distanceMantissa = frexp(scale, distanceExponent);
        distanceExponent += scaleShift;
        attenuation /= lengthSquared;
    }

    float noL = clamp(dot(normal, L), 0.0, 1.0);
    if (noL == 0.0 || attenuation == 0.0)
    {
        return vec3(0.0);
    }
    CoreBrdfLobes lobes = EvaluateCoreBrdf(pixel, normal, viewDirection, L);
    // BRDF 波瓣按 sr^-1 返回. NoL 在权重中乘一次, 强度, 曝光和距离尺度的指数在最后合并.
    int intensityExponent;
    float intensityMantissa = frexp(light.intensity, intensityExponent);
    float weight = (noL * attenuation) * (intensityMantissa * exposureMantissa) /
                   (distanceMantissa * distanceMantissa);
    return ScaleWorkingLight((lobes.diffuse + lobes.specular * specularCompensation) * light.color * weight,
                             intensityExponent + exposureExponent - 2 * distanceExponent);
}

// 以当帧预曝光自发光为起值. 两个有效段在同一数组中相邻, 计数和是首个无效行的下标.
// 零灯时循环为空. 直接光和自发光各带一次曝光, 两者都不使用材质 AO.
vec3 EvaluateLighting(PixelParams pixel, vec3 normal, vec3 viewDirection, LightingSurface surface, vec3 emission,
                      vec3 specularCompensation)
{
    int exponent;
    float exposure = frexp(perView.preExposure, exponent);
    vec3 result = ScaleWorkingLight(emission * exposure, exponent);
    uint count = perView.directionalCount + perView.localCount;
    for (uint index = 0u; index < count; ++index)
    {
        result += EvaluateDirect(lightRows.records[index], index < perView.directionalCount, pixel, normal,
                                 viewDirection, surface,
                                 exposure, exponent, specularCompensation);
    }
    return min(result, vec3(MaxWorkingLight));
}
