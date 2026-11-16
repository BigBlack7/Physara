#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>

// 同尺寸逐像素读取. 目标附件负责 sRGB 存储, UI 采样获得显示线性值.
void main()
{
    vec3 color = texelFetch(workingColor, ivec2(gl_FragCoord.xy), 0).rgb;
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
