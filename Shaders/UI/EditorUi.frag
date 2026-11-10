#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Fragment.glsl>

// SRV 格式决定采样时的颜色解码. 线性 RGB 相乘, 字形覆盖率乘顶点 alpha.
// 输出为 straight alpha. Pipeline 在线性域混合, sRGB 附件在存储时编码.
void main()
{
    outColor = linearColor * texture(sampler2D(uiImage, uiSampler), uv);
}
