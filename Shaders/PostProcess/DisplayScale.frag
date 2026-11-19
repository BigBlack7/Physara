#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>
#include <Source/PostProcess/DisplayImage.glsl>

// 只重采样已完成分级或诊断限幅的 RGB. 输出矩形以外写不透明底色, 不延展源图边缘.
void main()
{
    if (!InsideDisplayRect(gl_FragCoord.xy, displayRects.outputRect))
    {
        outColor = DisplayBackground;
        return;
    }
    // 两个有效矩形共享 UV. 像素中心的缩放只过滤片内线性 RGB, 终点由格式写入 sRGB.
    vec2 uv = (gl_FragCoord.xy - vec2(displayRects.outputRect.xy)) / vec2(displayRects.outputRect.zw);
    vec2 position = vec2(displayRects.renderRect.xy) + uv * vec2(displayRects.renderRect.zw);
    outColor = vec4(SampleDisplayImage(position).rgb, 1.0);
}
