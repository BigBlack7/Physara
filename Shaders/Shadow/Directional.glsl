// 场景位置、相机轴和每级光空间投影来自同一帧. 采样仅访问实际分配且已定义的单层视图.
#include <Source/Geometry/LightingPosition.glsl>

// first 等于有效级数表示超出覆盖. 非零 nextWeight 对应下一层的比较权重.
struct CascadeSelection
{
    uint first;
    float nextWeight;
};

// 内部区间按左闭右开选层. 过渡带同时覆盖相邻两级, 末级包含有限远端.
CascadeSelection SelectCascade(float viewDepth)
{
    if (!(viewDepth >= cascades.nearDepth && viewDepth <= cascades.farDepths[cascades.count - 1u]))
    {
        return CascadeSelection(cascades.count, 0.0);
    }
    for (uint level = 0u; level + 1u < cascades.count; ++level)
    {
        float start = cascades.transitionStarts[level];
        float end = cascades.transitionEnds[level];
        if (viewDepth >= start && viewDepth < end)
        {
            return CascadeSelection(level, (viewDepth - start) / (end - start));
        }
        if (viewDepth < cascades.farDepths[level])
        {
            return CascadeSelection(level, 0.0);
        }
    }
    return CascadeSelection(cascades.count - 1u, 0.0);
}

// 四个常规二维绑定分别解释一个物理层. 各级的存储分辨率相同.
float CascadeDepth(uint level, ivec2 texel)
{
    switch (level)
    {
    case 0u: return texelFetch(cascadeImage0, texel, 0).r;
    case 1u: return texelFetch(cascadeImage1, texel, 0).r;
    case 2u: return texelFetch(cascadeImage2, texel, 0).r;
    default: return texelFetch(cascadeImage3, texel, 0).r;
    }
}

// 查询矩阵保留接收点的原始深度. 翻转 NDC 的 Y 后按左上纹理原点取单个 texel, 等深返回可见.
float DirectionalVisibility(uint level, LightingSurface surface)
{
    LightingPosition origin = LightingPosition(cascades.originHigh[level].xyz, cascades.originLow[level].xyz);
    vec3 clip = (cascadeMatrices.clipFromOrigin[level] * vec4(-LightingDelta(origin, surface, 1.0), 1.0)).xyz;
    vec2 uv = clip.xy * vec2(0.5, -0.5) + 0.5;
    if (!all(greaterThanEqual(uv, vec2(0.0))) || !all(lessThan(uv, vec2(1.0))) ||
        !(clip.z >= 0.0 && clip.z <= 1.0))
    {
        return 1.0;
    }
    ivec2 texel = ivec2(uv * float(cascades.edge));
    return clip.z <= CascadeDepth(level, texel) ? 1.0 : 0.0;
}
