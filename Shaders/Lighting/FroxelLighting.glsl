#ifndef PHYSARA_FROXEL_LIGHTING_GLSL
#define PHYSARA_FROXEL_LIGHTING_GLSL

#include <Source/Lighting/Froxel.glsl>
#include <Source/Lighting/Punctual.glsl>

// renderPixel 是渲染画布左上像素坐标, viewDepth 是米制正向视深. 四个 Froxel 块和灯表属于同一 View.
// 两张局部索引表互斥, 行号同时选择灯记录和阴影引用. 正常空格仍消费全局局部灯.
// 无有效格和溢出格沿完整灯入口求值, 不读取紧凑索引及全局局部灯数组.
vec3 EvaluateFroxelLighting(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, LightingSurface surface,
                            vec3 emission, vec3 specularCompensation, vec2 renderPixel, float viewDepth)
{
    uint cell = QueryFroxelCell(renderPixel, viewDepth);
    if (cell != FroxelInvalidCell)
    {
        FroxelCellHeader header = froxelHeaders.records[cell];
        if (header.overflow == 0u)
        {
            float exposure;
            int exponent;
            vec3 result = EvaluateLightingPrefix(pixel, geometry, viewDirection, surface, emission,
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
    return EvaluateLighting(pixel, geometry, viewDirection, surface, emission, specularCompensation);
}

#endif
