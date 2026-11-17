#ifndef PHYSARA_UNIT_DIRECTION_GLSL
#define PHYSARA_UNIT_DIRECTION_GLSL

// 无资源依赖的方向归一化, 供材质解码, 几何框架及 BRDF 半角共同使用.
// 先按最大分量归一化, 避免可表示的方向在平方长度中溢出或下溢.
// value 和 fallback 须有限; value 全零时原样返回 fallback, 不再将其规范化.
// 调用方可传单位方向作为几何回退, 或传零向量作为无有效切线/半角的哨兵.
vec3 UnitOr(vec3 value, vec3 fallback)
{
    float scale = max(max(abs(value.x), abs(value.y)), abs(value.z));
    return scale > 0.0 ? normalize(value / scale) : fallback;
}

#endif
