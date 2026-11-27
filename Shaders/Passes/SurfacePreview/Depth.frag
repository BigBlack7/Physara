#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#include <Interface/Fragment.glsl>

// 预览在源有效矩形内按同一像素读取 D32. 灰阶保留原生 Reverse-Z 数值, 包括合法的零深度.
void main()
{
    float depth = texelFetch(previewDepth, ivec2(gl_FragCoord.xy), 0).r;
    outColor = vec4(vec3(depth), 1.0);
}
