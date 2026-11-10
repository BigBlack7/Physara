#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Vertex.glsl>

// 顶点阶段按生成接口读取逻辑位置, UV 和归一化创作色, 输出供片元插值.
// sRGB RGB 通道按阈值选择线性段或幂函数段. alpha 保留原来的线性值.
vec3 DecodeSrgb(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(color, vec3(0.04045)));
}

void main()
{
    // 投影常量将逻辑左上角映射到 NDC (-1, +1). UV 沿纹理逻辑行序透传.
    gl_Position = vec4(inPosition * uiProjection.scale + uiProjection.bias, 0.0, 1.0);
    uv = inUv;
    linearColor = vec4(DecodeSrgb(inColor.rgb), inColor.a);
}
