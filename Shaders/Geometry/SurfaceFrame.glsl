// 由插值后的逆转置法线与仿射切线构造像素框架, 输出世界空间单位方向.
// 先按最大分量归一化, 避免可表示的仿射方向在平方长度中溢出或下溢.
vec3 UnitOr(vec3 value, vec3 fallback)
{
    float scale = max(max(abs(value.x), abs(value.y)), abs(value.z));
    return scale > 0.0 ? normalize(value / scale) : fallback;
}

// 双面翻转作用在已经构造的着色法线上. normal 纹理的 XY 缩放和 GPU 框架朝向独立.
vec3 EvaluateSurfaceNormal(vec3 normalSample, bool usesNormalMap)
{
    vec3 normal = UnitOr(vNormal, vec3(0.0, 0.0, 1.0));
    if (usesNormalMap)
    {
        vec3 tangent = UnitOr(vTangent.xyz, vec3(0.0));
        tangent = UnitOr(tangent - normal * dot(normal, tangent), vec3(0.0));
        vec3 bitangent = cross(normal, tangent) * (vTangent.w < 0.0 ? -1.0 : 1.0);
        vec3 local = normalSample * 2.0 - 1.0;
        local.xy *= material.surface.w;
        local = UnitOr(local, vec3(0.0, 0.0, 1.0));
        // 有效角点可在像素插值时相消. 此处使用同一插值几何法线作为退化像素的结果.
        normal = UnitOr(mat3(tangent, bitangent, normal) * local, normal);
    }
    return gl_FrontFacing ? normal : -normal;
}
