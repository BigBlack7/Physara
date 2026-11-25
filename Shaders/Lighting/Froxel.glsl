#ifndef PHYSARA_LIGHTING_FROXEL_GLSL
#define PHYSARA_LIGHTING_FROXEL_GLSL

// 查询需要生成的 Fragment.glsl 提供 froxelView. 返回值是 header 下标, 调用方须先判定哨兵再索引.
const uint FroxelInvalidCell = 0xffffffffu;

// 以 FP32 的符号和绝对值位型比较坐标, 正负零使用相同的顺序值.
ivec2 FroxelOrderedPixels(uvec2 bits)
{
    ivec2 magnitude = ivec2(bits & uvec2(0x7fffffffu));
    return mix(magnitude, -magnitude, greaterThanEqual(bits, uvec2(0x80000000u)));
}

// pixel 相对去抖动的有效矩形左上角, viewDepth 为米制 -viewZ.
// 只有稳定外缘内的留量可以归入边缘 tile. 内部边界向后归属, 最后一层包含远端.
uint QueryStableFroxelCell(vec2 pixel, float viewDepth)
{
    // 某些设备将负次正规值比较为零. 按位拒绝负的非零值, 同时允许正交零近面.
    if (floatBitsToUint(viewDepth) > 0x80000000u)
    {
        return FroxelInvalidCell;
    }
    if (!all(greaterThanEqual(pixel, froxelView.stableMinimum)) ||
        !all(lessThanEqual(pixel, froxelView.stableMaximum)) ||
        !(viewDepth >= froxelView.depthEdges[0] && viewDepth <= froxelView.depthEdges[froxelView.sliceCount]))
    {
        return FroxelInvalidCell;
    }

    // 取整前先夹到有效像素尺度. 两端的 guard band 使用最外一列或行.
    uvec2 tile = uvec2(clamp(pixel, vec2(0.0), vec2(froxelView.renderRect.zw))) / froxelView.tileExtent;
    tile = min(tile, froxelView.tileCount - uvec2(1));

    // 直接读取 CPU 发布的边界, 同一份比较规则覆盖正交和透视, 无需在查询时重新近似对数.
    uint low = 0u;
    uint high = froxelView.sliceCount;
    while (high - low > 1u)
    {
        uint middle = (low + high) / 2u;
        if (viewDepth >= froxelView.depthEdges[middle])
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return (low * froxelView.tileCount.y + tile.y) * froxelView.tileCount.x + tile.x;
}

// 已拥有可靠光栅坐标的消费者从画布像素查询同一个稳定网格.
uint QueryFroxelCell(vec2 renderPixel, float viewDepth)
{
    uvec2 origin = froxelView.renderRect.xy;
    precise vec2 relative = renderPixel - vec2(origin);
    bvec2 zeroEdge = equal(froxelView.stableMinimum, vec2(0.0));
    if (any(zeroEdge))
    {
        // 零原点直接读取画布坐标位型, 在减抖动前判断是否越过零外缘.
        uvec2 bits = mix(floatBitsToUint(relative), floatBitsToUint(renderPixel), equal(origin, uvec2(0u)));
        bvec2 below = lessThan(FroxelOrderedPixels(bits), FroxelOrderedPixels(floatBitsToUint(froxelView.jitterPixels)));
        if ((zeroEdge.x && below.x) || (zeroEdge.y && below.y))
        {
            return FroxelInvalidCell;
        }
    }
    precise vec2 pixel = relative - froxelView.jitterPixels;
    return QueryStableFroxelCell(pixel, viewDepth);
}

#endif
