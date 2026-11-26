// 最终 G3 决定核心资格, 因而有限远面上 D32 为零的几何仍会着色. 无资格处保留附件清除的底色和零覆盖.
#include <Interface/Fragment.glsl>
#include <Source/Material/CoreGBuffer.glsl>
#include <Source/Lighting/OpaqueLighting.glsl>

// 当前有效矩形以左上为原点. Reverse-Z 纹理值直接对应 [0,1] 的 clip 深度.
vec4 SurfaceNdc(ivec2 pixel, float depth)
{
    vec2 uv = (vec2(pixel) + 0.5 - vec2(deferredView.renderRect.xy)) / vec2(deferredView.renderRect.zw);
    return vec4(uv * vec2(2.0, -2.0) + vec2(-1.0, 1.0), depth, 1.0);
}

// 从光栅已写的 D32 和像素中心估计逆投影误差. 顶点阶段已在相机相对坐标中生成 clip 位置.
// 分母下界为正时换算为米制误差, 否则返回负哨兵, 调用方改查完整灯表.
float ReconstructionError(vec4 ndc, vec4 homogeneous, vec3 viewPosition)
{
    const float slack = 32.0 * 1.1920928955078125e-7;
    mat4 inverseProjection = deferredView.viewFromRenderClip;
    vec4 magnitude = abs(inverseProjection[0]) * (1.0 + abs(ndc.x)) +
                     abs(inverseProjection[1]) * (1.0 + abs(ndc.y)) +
                     abs(inverseProjection[2]) * abs(ndc.z) + abs(inverseProjection[3]);
    vec4 error = slack * magnitude;
    float w = abs(homogeneous.w) - error.w;
    return w > 0.0 ? dot((error.xyz + abs(viewPosition) * error.w) / w, vec3(1.0)) : -1.0;
}

#ifdef PHYSARA_FORWARD_SHADOWS
// 相邻样点从最终表面取得. 只让有效矩形内的核心几何为本法线提供差分.
bool ReceiverDepth(ivec2 pixel, out float depth)
{
    ivec2 relative = pixel - ivec2(deferredView.renderRect.xy);
    if (any(lessThan(relative, ivec2(0))) ||
        any(greaterThanEqual(relative, ivec2(deferredView.renderRect.zw))) ||
        !IsCoreSurface(texelFetch(gBufferG3, pixel, 0).r))
    {
        return false;
    }
    depth = texelFetch(gBufferDepth, pixel, 0).r;
    return true;
}

// 比较中心与同侧两个邻点的 D32 一阶差分. 在满足容差的候选中取残差较小的一侧, 两侧均不可用时返回零.
vec3 ReceiverTangent(ivec2 pixel, ivec2 axis, float centerDepth, vec3 center)
{
    vec3 tangent = vec3(0.0);
    float best = 1.0;
    for (int side = -1; side <= 1; side += 2)
    {
        ivec2 step = side * axis;
        float firstDepth, secondDepth;
        if (!ReceiverDepth(pixel + step, firstDepth) || !ReceiverDepth(pixel + 2 * step, secondDepth))
        {
            continue;
        }
        float residual = abs((secondDepth - firstDepth) - (firstDepth - centerDepth));
        float range = max(centerDepth, max(firstDepth, secondDepth));
        const float tolerance = 8.0 * 1.1920928955078125e-7;
        if (residual <= tolerance * range && residual < best)
        {
            vec4 samplePoint = deferredView.viewFromRenderClip * SurfaceNdc(pixel + step, firstDepth);
            if (samplePoint.w != 0.0)
            {
                tangent = (samplePoint.xyz / samplePoint.w - center) * float(side);
                best = residual;
            }
        }
    }
    return tangent;
}

// 与贴图无关的相机空间可见平面. 零向量在共享阴影查询中只取消法线偏移.
vec3 ReceiverNormal(ivec2 pixel, float depth, vec3 center, vec3 viewDirection)
{
    vec3 dx = ReceiverTangent(pixel, ivec2(1, 0), depth, center);
    vec3 dy = ReceiverTangent(pixel, ivec2(0, 1), depth, center);
    vec3 normal = UnitOr(deferredView.worldBasis * cross(dx, dy), vec3(0.0));
    return dot(normal, viewDirection) < 0.0 ? -normal : normal;
}
#endif

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    if (!IsCoreSurface(texelFetch(gBufferG3, pixel, 0).r))
    {
        discard;
    }
    CoreSurfaceSample material = DecodeCoreSurface(texelFetch(gBufferG0, pixel, 0),
                                                   texelFetch(gBufferG1, pixel, 0),
                                                   texelFetch(gBufferG2, pixel, 0).rgb);
    float depth = texelFetch(gBufferDepth, pixel, 0).r;
    vec4 ndc = SurfaceNdc(pixel, depth);
    vec4 homogeneous = deferredView.viewFromRenderClip * ndc;
    vec3 viewPosition = homogeneous.xyz / homogeneous.w;
    vec3 offset = deferredView.worldBasis * viewPosition;
    if (any(isnan(offset)) || any(isinf(offset)))
    {
        // 无穷远投影的不可表示位置仍保有实际覆盖和已定义的工作域自发光.
        outColor = vec4(material.preExposedEmission, 1.0);
        return;
    }
    // 以写入 D32 的眼点高位为基点. 将重建位移、眼点低位与上传余量分开交给相对作差.
    LightingSurface surface = LightingSurface(perView.originHigh,
        LightingPosition(offset, perView.originLow, perView.originResidual));
    vec3 viewDirection = perView.orthographic != 0u ? perView.orthographicDirection :
        UnitOr(-offset, material.shadingNormal);
    SurfaceGeometry geometry = SurfaceGeometry(vec3(0.0), material.shadingNormal);
#ifdef PHYSARA_FORWARD_SHADOWS
    geometry.geometricNormal = ReceiverNormal(pixel, depth, viewPosition, viewDirection);
#endif
    float positionError = ReconstructionError(ndc, homogeneous, viewPosition);
    uint cell = positionError >= 0.0 ? QuerySurfaceFroxelCell(surface, positionError) : FroxelInvalidCell;
    vec3 color = EvaluateOpaqueLighting(material.pixel, geometry, viewDirection, surface, material.preExposedEmission,
                                        material.occlusion, NoScreenLighting, cell);
    outColor = vec4(color, 1.0);
}
