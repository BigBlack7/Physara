// 后处理坐标统一在左上原点的像素空间. 留边用同一线性底色, 深度及覆盖仍归场景.
const vec4 DisplayBackground = vec4(0.018, 0.018, 0.018, 1.0);

// rect 是像素 (x, y, 宽, 高). 半开的右边和下边不属于图像.
bool InsideDisplayRect(vec2 center, uvec4 rect)
{
    return all(greaterThanEqual(center, vec2(rect.xy))) &&
           all(lessThan(center, vec2(rect.xy + rect.zw)));
}

// position 是渲染画布内的绝对像素坐标. 先夹到图像首末像素中心再除整张纹理尺寸,
// 否则双线性过滤即使位于纹理内, 也可能混入相机的留边.
vec4 SampleDisplayImage(vec2 position)
{
    vec2 first = vec2(displayRects.renderRect.xy) + 0.5;
    vec2 last = first + vec2(displayRects.renderRect.zw) - 1.0;
    vec2 uv = clamp(position, first, last) / vec2(displayRects.renderExtent);
    return textureLod(sampler2D(sourceColor, linearSampler), uv, 0.0);
}
