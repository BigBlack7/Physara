#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>

// 诊断限幅只逐像素读工作色. 需要缩放时写线性中间附件, 否则直接由最终 sRGB 附件编码.
void main()
{
    vec3 color = texelFetch(workingColor, ivec2(gl_FragCoord.xy), 0).rgb;
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
