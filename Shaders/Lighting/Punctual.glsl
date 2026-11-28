// 不透明表面的逐灯求值. 灯量保留 lux/candela, 距离为米, 输出为当轮预曝光线性工作色.
// 衰减采用项目的四次距离窗和 Khronos 余弦平方角衰减. 各材质波瓣由共享 BRDF 提供.
#include <Source/BRDF/CoreBrdf.glsl>
#include <Source/Geometry/LightingPosition.glsl>
#include <Source/Geometry/SurfaceGeometry.glsl>
#include <Source/Lighting/WorkingLight.glsl>
#ifdef PHYSARA_SHEEN
#include <Source/BRDF/SheenBrdf.glsl>
#endif
#ifdef PHYSARA_FORWARD_SHADOWS
#include <Source/Shadow/Visibility.glsl>
#endif

// 距离按最大分量缩放, 避免 r^2 自身的溢出及除以零. D 为发射轴, L 由表面指向灯.
vec3 EvaluateDirect(ForwardLight light, uint lightIndex, bool directional, PixelParams pixel,
                    SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                    float exposureMantissa, int exposureExponent, SpecularCompensation specularCompensation)
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
        // 高位携带灯位符号和指数. 剩余 29 位按 24 位低位和 5 位余量还原, 与高位不重叠.
        const int headDigits = 24;
        const int sourceDigits = 53;
        const int residualDigits = sourceDigits - 2 * headDigits;
        const uint residualMask = (1u << residualDigits) - 1u;
        ivec3 exponent;
        frexp(light.positionHigh, exponent);
        vec3 positionSign = sign(light.positionHigh);
        vec3 low = ldexp(positionSign * vec3(light.positionTail >> residualDigits), exponent - 2 * headDigits);
        vec3 residual = ldexp(positionSign * vec3(light.positionTail & residualMask), exponent - sourceDigits);
        // 三项分别进入与表面的相对作差. 若在世界坐标中先把尾数加回高位, FP32 舍入会丢失这部分位移.
        LightingPosition lightPosition = LightingPosition(light.positionHigh, low, residual);
        vec3 delta = LightingDelta(lightPosition, surface, 1.0);
        int scaleShift = 0;
        if (any(isinf(delta)) || any(isnan(delta)))
        {
            // 各位置项缩小到四分之一后重新相减, 再判断有限范围. 指数补回 2 以恢复米制距离.
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

    float noL = clamp(dot(geometry.shadingNormal, L), 0.0, 1.0);
#ifdef PHYSARA_CLEARCOAT
    float coatNoL = 0.0;
    if (pixel.coatWeight > 0.0 && CoatFacesView(geometry, viewDirection) &&
        dot(geometry.facingGeometricNormal, L) > 0.0)
    {
        coatNoL = clamp(dot(geometry.coatNormal, L), 0.0, 1.0);
    }
    if ((noL == 0.0 && coatNoL == 0.0) || attenuation == 0.0)
#else
    if (noL == 0.0 || attenuation == 0.0)
#endif
    {
        return vec3(0.0);
    }
#ifdef PHYSARA_FORWARD_SHADOWS
    // 本灯的可见性只改变它的直接入射量, 反射波瓣继续使用材质法线.
    attenuation *= QueryShadow(shadowReferences.records[lightIndex].group,
                               surface, geometry.geometricNormal).visibility;
#endif
    CoreBrdfLobes lobes = EvaluateCoreBrdf(pixel, geometry.shadingNormal, viewDirection, L);
    // BRDF 波瓣按 sr^-1 返回. NoL 在权重中乘一次, 强度, 曝光和距离尺度的指数在最后合并.
    int intensityExponent;
    float intensityMantissa = frexp(light.intensity, intensityExponent);
    float weight = (noL * attenuation) * (intensityMantissa * exposureMantissa) /
                   (distanceMantissa * distanceMantissa);
    vec3 specular = lobes.specular * specularCompensation.base;
    if (UsesIorSpecular(pixel))
    {
        specular += lobes.metalSpecular * specularCompensation.metal;
    }
    vec3 response = (lobes.diffuse + specular) * light.color * weight;
#ifdef PHYSARA_SHEEN
    // weight 已含基底法线的 NoL, sheen 共用此余弦. 清漆随后衰减这两个已合成的下层波瓣.
    response *= SheenBaseTransmission(pixel);
    if (pixel.sheenScale > 0.0 && noL > 0.0 && dot(geometry.facingGeometricNormal, L) > 0.0)
    {
        float sheen = EvaluateSheenBrdf(pixel, geometry.shadingNormal, viewDirection, L, noL);
        response += light.color * pixel.sheenColor * (sheen * weight);
    }
#endif
#ifdef PHYSARA_CLEARCOAT
    response *= 1.0 - pixel.coatWeight;
    if (coatNoL > 0.0)
    {
        float coat = EvaluateCoatBrdf(pixel, geometry.coatNormal, viewDirection, L, coatNoL);
        float coatIncident = (coatNoL * attenuation) * (intensityMantissa * exposureMantissa) /
                             (distanceMantissa * distanceMantissa);
        response += light.color * (coat * specularCompensation.coat * pixel.coatWeight * coatIncident);
    }
#endif
    return ScaleWorkingLight(response, intensityExponent + exposureExponent - 2 * distanceExponent);
}

// 以预曝光自发光为起值, 按完整灯表的前缀顺序累加. 完整模式使用两个有效段, Froxel 使用方向灯段.
// count 是从第零行起读取的条目数. 返回累加值及预曝光的尾数和指数, 供调用方继续逐行累加后限幅.
vec3 EvaluateLightingPrefix(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection,
                            LightingSurface surface, vec3 preExposedEmission, SpecularCompensation specularCompensation,
                            uint count, out float exposure, out int exponent)
{
    exposure = frexp(perView.preExposure, exponent);
    vec3 result = preExposedEmission;
    for (uint index = 0u; index < count; ++index)
    {
        result += EvaluateDirect(lightRows.records[index], index, index < perView.directionalCount, pixel, geometry,
                                 viewDirection, surface,
                                 exposure, exponent, specularCompensation);
    }
    return result;
}

// 完整灯模式也是 Froxel 不可查询或溢出时的着色入口. 自发光以工作域值进入累加.
vec3 EvaluateLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                      vec3 preExposedEmission, SpecularCompensation specularCompensation)
{
    float exposure;
    int exponent;
    return min(EvaluateLightingPrefix(pixel, geometry, viewDirection, surface, preExposedEmission, specularCompensation,
                                      perView.directionalCount + perView.localCount, exposure, exponent),
               vec3(MaxWorkingLight));
}
