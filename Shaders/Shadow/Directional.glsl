// 级联使用正交投影, w 恒为 1. 相对原点的位置与米制偏移分别投影, 以该级自己的世界 texel 决定偏移量.
float CascadeVisibility(uint layer, ForwardShadowLayer entry, LightingSurface surface,
                        vec3 geometricNormal, vec3 toLight)
{
    LightingPosition origin = LightingPosition(entry.originHigh, entry.originLow, entry.originResidual);
    vec3 position = -LightingDelta(origin, surface, 1.0);
    vec3 offset = ShadowReceiverOffset(geometricNormal, toLight, 0.5 * entry.worldTexel);
    vec3 clip = (entry.clipFromOrigin * vec4(position, 1.0)).xyz +
                (entry.clipFromOrigin * vec4(offset, 0.0)).xyz;
    bool valid;
    return FilterShadow(clip, layer, valid);
}

// 内部区间按左闭右开选层. 过渡带混合两次独立过滤的可见性, 末级包含有限远端.
ShadowResult QueryDirectionalShadow(uint base, ForwardShadowLayer group, LightingSurface surface,
                                    vec3 geometricNormal)
{
    LightingPosition camera = LightingPosition(perView.originHigh, perView.originLow, perView.originResidual);
    float depth = -dot(shadowView.cameraForward, LightingDelta(camera, surface, 1.0));
    if (!(depth >= group.nearDepth && depth <= shadowLayers.records[base + group.count - 1u].farDepth))
    {
        return OutsideShadow;
    }
    for (uint level = 0u; level < group.count; ++level)
    {
        uint layer = base + level;
        ForwardShadowLayer entry = shadowLayers.records[layer];
        vec2 transition = entry.transition;
        if (level + 1u < group.count && depth >= transition.x && depth < transition.y)
        {
            float weight = (depth - transition.x) / (transition.y - transition.x);
            float from = CascadeVisibility(layer, entry, surface, geometricNormal, group.lightDirection);
            float to = CascadeVisibility(layer + 1u, shadowLayers.records[layer + 1u], surface,
                                         geometricNormal, group.lightDirection);
            return ShadowResult(int(level), weight, mix(from, to, weight));
        }
        if (depth < entry.farDepth || level + 1u == group.count)
        {
            return ShadowResult(int(level), 0.0,
                CascadeVisibility(layer, entry, surface, geometricNormal, group.lightDirection));
        }
    }
    return OutsideShadow;
}
