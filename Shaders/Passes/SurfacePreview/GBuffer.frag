#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#include <Interface/Fragment.glsl>
#include <Source/Material/OctNormal.glsl>

// 标签来自生产 GBuffer 的 ABI. 辅助覆盖只交付分类、法线和粗糙度, 核心字段必须经过资格判定.
void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    uint route = texelFetch(previewG3, pixel, 0).r;
    uvec4 controls = previewControls.channelAndRoutes;
    bool core = route == controls.y;
    bool unlit = route == controls.z;
    bool extension = route == controls.w;
    if (!core && !unlit && !extension)
    {
        discard;
    }

    vec3 color;
    switch (controls.x)
    {
    case PreviewRoute:
        color = core ? vec3(0.0, 0.75, 1.0) : (unlit ? vec3(1.0, 0.4, 0.0) : vec3(0.7, 0.1, 1.0));
        break;
    case PreviewNormal:
        color = 0.5 * DecodeCoreNormal(texelFetch(previewG1, pixel, 0).xy) + 0.5;
        break;
    case PreviewRoughness:
        color = vec3(texelFetch(previewG1, pixel, 0).z);
        break;
    default:
        if (!core)
        {
            discard;
        }
        switch (controls.x)
        {
        case PreviewBaseColor: color = texelFetch(previewG0, pixel, 0).rgb; break;
        case PreviewOcclusion: color = vec3(texelFetch(previewG0, pixel, 0).a); break;
        case PreviewMetallic: color = vec3(texelFetch(previewG1, pixel, 0).w); break;
        case PreviewEmission:
            // G2 已应用预曝光. 逐通道压缩超白, 由 sRGB 颜色附件完成显示编码.
            color = texelFetch(previewG2, pixel, 0).rgb;
            color /= 1.0 + color;
            break;
        default: discard;
        }
        break;
    }
    outColor = vec4(color, 1.0);
}
