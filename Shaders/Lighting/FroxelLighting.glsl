#ifndef PHYSARA_FROXEL_LIGHTING_GLSL
#define PHYSARA_FROXEL_LIGHTING_GLSL

#include <Source/Lighting/Froxel.glsl>
#include <Source/Lighting/Punctual.glsl>

// XY 已取右、下两个视空间轴, 投到相对矩形的稳定像素. w 在正交时为 1, 透视时为正视深.
vec2 FroxelStablePixel(vec2 xy, vec2 w)
{
    vec2 halfExtent = 0.5 * vec2(froxelView.renderRect.zw);
    return fma((xy / w) * froxelView.projectionScale, halfExtent, halfExtent);
}

// 与逐灯求值共用表面的世界基点和插值余量. 先相对眼点求差, 再在稳定投影下定位格子.
// 格查询使用稳定投影和同一受光位置的误差界. 当前 gl_Position 另含渲染抖动.
uint QuerySurfaceFroxelCell(LightingSurface surface, float positionError)
{
    LightingPosition camera = LightingPosition(froxelView.cameraHigh, froxelView.cameraLow, froxelView.cameraResidual);
    vec3 roundoff;
    vec3 position = froxelView.viewBasis * -LightingDelta(camera, surface, 1.0, roundoff);
    float depth = -position.z;
    bool orthographic = froxelView.orthographic != 0u;
    float w = orthographic ? 1.0 : depth;
    if (!(w > 0.0) || !all(greaterThan(froxelView.projectionScale, vec2(0.0))) ||
        any(isinf(position)) || any(isnan(position)))
    {
        return FroxelInvalidCell;
    }
    vec2 xy = vec2(position.x, -position.y);
    float lost = dot(roundoff, vec3(1.0)) + positionError;
    if (lost == 0.0)
    {
        return QueryStableFroxelCell(FroxelStablePixel(xy, vec2(w)), depth);
    }
    if (!(lost > 0.0) || isinf(lost))
    {
        return FroxelInvalidCell;
    }

    // 世界轴绝对余差之和界定旋转后各轴的误差. 留出求和、旋转和区间端点的 FP32 舍入量.
    const float slack = 32.0 * 1.1920928955078125e-7;
    vec3 magnitude = abs(position);
    float bound = lost * (1.0 + slack) + slack * max(magnitude.x, max(magnitude.y, magnitude.z));
    float nearDepth = depth - bound;
    float farDepth = depth + bound;
    if (!orthographic && !(nearDepth > 0.0))
    {
        return FroxelInvalidCell;
    }
    vec2 lower = xy - vec2(bound);
    vec2 upper = xy + vec2(bound);
    // 深度分母为正, 按坐标符号选取能产生各投影极值的近端或远端.
    vec2 lowerW = orthographic ? vec2(1.0) : mix(vec2(nearDepth), vec2(farDepth), greaterThan(lower, vec2(0.0)));
    vec2 upperW = orthographic ? vec2(1.0) : mix(vec2(farDepth), vec2(nearDepth), greaterThan(upper, vec2(0.0)));
    uint nearCell = QueryStableFroxelCell(FroxelStablePixel(lower, lowerW), nearDepth);
    uint farCell = QueryStableFroxelCell(FroxelStablePixel(upper, upperW), farDepth);
    // 两次查询分别取像素和视深的各轴下界与上界. 两端同属一个有效格时, 整个区间都可用该格的候选.
    return nearCell == farCell ? nearCell : FroxelInvalidCell;
}

uint QuerySurfaceFroxelCell(LightingSurface surface)
{
    return QuerySurfaceFroxelCell(surface, 0.0);
}

// 四个 Froxel 块和灯表属于同一 View. cell 来自同一表面坐标的查询, 可携带无效格哨兵.
// 两张局部索引表互斥, 行号同时选择灯记录和阴影引用. 正常空格仍消费全局局部灯.
// 无有效格和溢出格沿完整灯入口求值, 不读取紧凑索引及全局局部灯数组.
vec3 EvaluateFroxelLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                            vec3 preExposedEmission, SpecularCompensation specularCompensation, uint cell)
{
    if (cell != FroxelInvalidCell)
    {
        FroxelCellHeader header = froxelHeaders.records[cell];
        if (header.overflow == 0u)
        {
            float exposure;
            int exponent;
            vec3 result = EvaluateLightingPrefix(pixel, geometry, viewDirection, surface, preExposedEmission,
                                                 specularCompensation, perView.directionalCount, exposure, exponent);
            // 方向灯和自发光已计入 result. 两表按绝对行号归并, 保留与全灯退路相同的累加顺序.
            const uint noRow = 0xffffffffu;
            uint globalCount = froxelView.globalCount;
            uint globalIndex = 0u;
            uint finiteIndex = header.offset;
            uint finiteEnd = finiteIndex + header.count;
            uint globalRow = globalIndex < globalCount ? froxelGlobalLights.records[globalIndex].lightRow : noRow;
            uint finiteRow = finiteIndex < finiteEnd ? froxelIndices.records[finiteIndex].lightRow : noRow;
            while (globalRow != noRow || finiteRow != noRow)
            {
                uint row = min(globalRow, finiteRow);
                result += EvaluateDirect(lightRows.records[row], row, false, pixel, geometry, viewDirection,
                                         surface, exposure, exponent, specularCompensation);
                if (globalRow < finiteRow)
                {
                    ++globalIndex;
                    globalRow = globalIndex < globalCount ? froxelGlobalLights.records[globalIndex].lightRow : noRow;
                }
                else
                {
                    ++finiteIndex;
                    finiteRow = finiteIndex < finiteEnd ? froxelIndices.records[finiteIndex].lightRow : noRow;
                }
            }
            return min(result, vec3(MaxWorkingLight));
        }
    }
    return EvaluateLighting(pixel, geometry, viewDirection, surface, preExposedEmission, specularCompensation);
}

// 已有可靠像素和视深的调用方沿原画布查询入口选择候选.
vec3 EvaluateFroxelLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                            vec3 preExposedEmission, SpecularCompensation specularCompensation,
                            vec2 renderPixel, float viewDepth)
{
    return EvaluateFroxelLighting(pixel, geometry, viewDirection, surface, preExposedEmission, specularCompensation,
                                  QueryFroxelCell(renderPixel, viewDepth));
}

#endif
