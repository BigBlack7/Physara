// 点光面顺序为 +X,-X,+Y,-Y,+Z,-Z. 名义面边和交角由扩展投影提供整个比较核.
uint PointShadowFace(vec3 delta, vec3 magnitude)
{
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
    {
        return delta.x >= 0.0 ? 0u : 1u;
    }
    if (magnitude.y >= magnitude.z)
    {
        return delta.y >= 0.0 ? 2u : 3u;
    }
    return delta.z >= 0.0 ? 4u : 5u;
}

// 原接收点决定有限半径、外锥和选面. 偏移后的齐次投影同时给出取样位置和常规深度.
ShadowResult QueryLocalShadow(uint base, ForwardShadowLayer group, LightingSurface surface, vec3 geometricNormal)
{
    LightingPosition origin = LightingPosition(group.originHigh, group.originLow);
    vec3 delta = -LightingDelta(origin, surface, 1.0);
    vec3 magnitude = abs(delta);
    float largest = max(magnitude.x, max(magnitude.y, magnitude.z));
    if (!(largest > 0.0 && largest <= group.farPlane))
    {
        return OutsideShadow;
    }
    vec3 scaled = delta / group.farPlane;
    if (!(dot(scaled, scaled) <= 1.0))
    {
        return OutsideShadow;
    }
    bool point = group.kind == 1u;
    uint face = point ? PointShadowFace(delta, magnitude) : 0u;
    mat4 projection = shadowLayers.records[base + face].clipFromOrigin;
    vec4 clip = projection * vec4(delta, 1.0);
    if (!(clip.w >= group.nearDepth && clip.w <= group.farPlane))
    {
        return OutsideShadow;
    }
    vec2 ndc = clip.xy / clip.w;
    // 外锥在扩大后的投影中仍为圆, 直接使用横向投影可区分极窄锥边界.
    if (!point && !(dot(ndc, ndc) <= shadowView.innerScale * shadowView.innerScale))
    {
        return OutsideShadow;
    }
    vec3 toLight = normalize(-delta / largest);
    // CPU 把投影张角与边长折入比例. 乘原点轴深得到米制上界, 扩面边带容纳偏移后的整个核.
    vec3 offset = ShadowReceiverOffset(geometricNormal, toLight, clip.w * group.receiverOffsetRatio);
    vec4 shift = projection * vec4(offset, 0.0);
    // 只缩短偏移, 保持原点的近远面准入. 近端没有可用空间时沿用原投影.
    if (shift.w < 0.0)
    {
        shift *= min(1.0, (clip.w - group.nearDepth) / -shift.w);
    }
    else if (shift.w > 0.0)
    {
        shift *= min(1.0, (group.farPlane - clip.w) / shift.w);
    }
    clip += shift;
    vec3 projected = clip.xyz / clip.w;
    projected.z = clamp(projected.z, 0.0, 1.0);
    bool valid;
    float visibility = FilterShadow(projected, base + face, valid);
    return valid ? ShadowResult(int(face), 0.0, visibility) : OutsideShadow;
}
