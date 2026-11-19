#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>
#include <Source/PostProcess/DisplayImage.glsl>

// 上游 Alexa LogC EI 1000 曲线. 归一化黑点和 B 轴切片位置配对 CPU atlas.
vec3 LutCoordinate(vec3 color)
{
    const float a = 5.555556;
    const float b = 0.047996;
    const float c = 0.244161 / log2(10.0);
    const float d = 0.386036;
    const float black = c * log2(b) + d;
    vec3 shaper = c * log2(a * clamp(color, vec3(0.0), vec3(58.856633)) + b) + d;
    return 63.0 * clamp((shaper - black) / (1.0 - black), vec3(0.0), vec3(1.0));
}

// R/G 只在一张 64 x 64 切片内双线性插值. 半像素使格点落在 texel 中心而不是相邻切片边界.
vec3 SampleLutSlice(vec2 rg, int layer)
{
    ivec2 tile = ivec2(layer % 8, layer / 8) * 64;
    vec2 uv = (vec2(tile) + rg + 0.5) / 512.0;
    return textureLod(sampler2D(colorLut, linearSampler), uv, 0.0).rgb;
}

void main()
{
    if (!InsideDisplayRect(gl_FragCoord.xy, displayRects.renderRect))
    {
        outColor = DisplayBackground;
        return;
    }
    // 工作色已经包含本帧预曝光. LUT 中 RGB 为 SDR 线性值, 无额外曝光或传递函数.
    vec3 hdr = texelFetch(sourceColor, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 grid = LutCoordinate(hdr);
    int low = int(grid.b);
    vec3 rgb = mix(SampleLutSlice(grid.rg, low), SampleLutSlice(grid.rg, min(low + 1, 63)), fract(grid.b));
    // 仅有后续 FXAA 时用 A 携带感知亮度; 它不是几何覆盖率, 也不是交给 UI 的透明度.
    float alpha = displayRects.encodeLuma != 0u ?
        sqrt(dot(rgb, vec3(0.2126, 0.7152, 0.0722))) : 1.0;
    outColor = vec4(rgb, alpha);
}
