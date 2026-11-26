// 两个物理变体以 NoV 和过滤后 p 查同一 DFG. 表的 R 是 B, G 是 A+B, 核心 BRDF 由此计算能量项.
CoreDfgTerms PrepareCoreDfg(PixelParams pixel, vec3 normal, vec3 viewDirection)
{
    float noV = CoreNormalViewCosine(normal, viewDirection);
    vec2 dfg = textureLod(sampler2D(dfgImage, environmentSampler), vec2(noV, pixel.perceptualRoughness), 0.0).rg;
    return EvaluateCoreDfg(pixel, dfg);
}

#if PHYSARA_PHYSICAL_LIGHTING == 2
// SH9 已含余弦卷积和 Lambert 的 1/pi. 正样点在源空间查询, 系数的尺度随后并入曝光.
vec3 EvaluateEnvironmentSh(vec3 n)
{
    return max(vec3(0.0), environment.sh[0].rgb +
        environment.sh[1].rgb * n.y + environment.sh[2].rgb * n.z + environment.sh[3].rgb * n.x +
        environment.sh[4].rgb * (n.y * n.x) + environment.sh[5].rgb * (n.y * n.z) +
        environment.sh[6].rgb * (3.0 * n.z * n.z - 1.0) + environment.sh[7].rgb * (n.z * n.x) +
        environment.sh[8].rgb * (n.x * n.x - n.y * n.y));
}

// 固定 Filament 的漫反射多反弹 AO 近似, 材质无遮挡时返回单位可见性.
vec3 DiffuseVisibility(float a, vec3 albedo)
{
    vec3 x = 2.0404 * albedo - 0.3324;
    vec3 y = -4.7951 * albedo + 0.6417;
    vec3 z = 2.7552 * albedo + 0.6903;
    return min(max(vec3(a), ((a * x + y) * a + z) * a), vec3(1.0));
}

// 漫反射与镜面共享环境逆旋转, 只将 AO 应用于间接分量.
vec3 EvaluateIbl(PixelParams pixel, vec3 normal, vec3 viewDirection, float ao, CoreDfgTerms terms,
                 float reflectionConfidence, vec3 preExposedReflection)
{
    float noV = CoreNormalViewCosine(normal, viewDirection);
    vec3 diffuse = EvaluateEnvironmentSh(environment.environmentFromWorld * normal);
    vec3 diffuseFactor = pixel.diffuseColor * terms.indirectDiffuseBudget * DiffuseVisibility(ao, pixel.diffuseColor);

    vec3 reflected = mix(reflect(-viewDirection, normal), normal, pixel.alpha * pixel.alpha);
    // 主导方向在粗糙表面可能退化, 这时取同一着色法线方向.
    vec3 direction = environment.environmentFromWorld * UnitOr(reflected, normal);
    float p = pixel.perceptualRoughness;
    // 与 CPU 为目标层反解的粗糙度配对, 因此此处的 p 是感知粗糙度.
    float lod = environment.maxMip * p * (2.0 - p);
    vec3 specular = textureLod(samplerCube(reflectionImage, environmentSampler), direction, lod).rgb;
    float specularAo = clamp(pow(noV + ao, exp2(-16.0 * pixel.alpha - 1.0)) - 1.0 + ao, 0.0, 1.0);
    vec3 specularFactor = terms.directionalAlbedo * terms.specularCompensation * specularAo;

    ivec3 exponent = ivec3(environment.radianceExponent);
    // 屏幕反射已乘置信度与预曝光. 两路镜面使用同一核心波瓣权重, 环境标定只影响环境输入.
    vec3 reflectedLight = ScaleEnvironmentLight(specular, specularFactor, environment.radianceMantissa, exponent) *
                          (1.0 - reflectionConfidence) + preExposedReflection * specularFactor;
    return ScaleEnvironmentLight(diffuse, diffuseFactor, environment.radianceMantissa,
                                 exponent + environment.shExponent) + reflectedLight;
}
#endif
