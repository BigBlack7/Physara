#ifndef PHYSARA_OCT_NORMAL_GLSL
#define PHYSARA_OCT_NORMAL_GLSL

// 上下半球共用有符号八面体坐标. 折叠边界给零分量取正号, 保留负 Z 极点.
vec2 FoldOctNormal(vec2 xy)
{
    return (1.0 - abs(xy.yx)) * mix(vec2(-1.0), vec2(1.0), greaterThanEqual(xy, vec2(0.0)));
}

// 输入为完成纹理和背面处理的单位世界法线, 输出写入 G1 的两个有符号通道.
vec2 EncodeCoreNormal(vec3 normal)
{
    vec2 xy = normal.xy / dot(abs(normal), vec3(1.0));
    return normal.z < 0.0 ? FoldOctNormal(xy) : xy;
}

// 将量化后的两个通道展开并重新归一化, 核心光照和诊断共用此解码.
vec3 DecodeCoreNormal(vec2 encoded)
{
    vec3 normal = vec3(encoded, 1.0 - abs(encoded.x) - abs(encoded.y));
    if (normal.z < 0.0)
    {
        normal.xy = FoldOctNormal(normal.xy);
    }
    return normalize(normal);
}

#endif
