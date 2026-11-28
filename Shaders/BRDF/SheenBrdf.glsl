// glTF sheen 的 Charlie/Neubelt 波瓣及当前方向的能量预算.
// 使用和固定 DFG 蓝通道一致的分布, 见 Docs/Architecture.md 8.5 和对应公式的参考索引.

// 同一视向预算供直接及间接基底受光使用. 自发光另沿清漆出射权重处理.
float SheenBaseTransmission(PixelParams pixel)
{
    return 1.0 - MaxReflectance(pixel.sheenColor) * pixel.sheenAlbedo;
}

// 输入是同一表面的有效 N/L/V. 调用前已验证两个方向在几何及着色半球内.
// 返回单位色的 Charlie/Neubelt 响应, 不含 NoL. q 约束窄掠射波瓣的方向反照率.
float EvaluateSheenBrdf(PixelParams pixel, vec3 normal, vec3 viewDirection, vec3 lightDirection, float noL)
{
    vec3 halfDirection = UnitOr(viewDirection + lightDirection, normal);
    // 用叉积直接求 sin²(thetaH), 避免半角接近法线时用 1 - NoH² 相减损失精度.
    vec3 tangentHalf = cross(normal, halfDirection);
    float sinSquared = min(dot(tangentHalf, tangentHalf), 1.0);
    float inverseAlpha = 1.0 / pixel.sheenAlpha;
    const float inverseTwoPi = 0.15915494309189535;
    float distribution = (2.0 + inverseAlpha) * pow(sinSquared, 0.5 * inverseAlpha) * inverseTwoPi;
    float noV = CoreNormalViewCosine(normal, viewDirection);
    return pixel.sheenScale * distribution / (4.0 * (noL + noV - noL * noV));
}
