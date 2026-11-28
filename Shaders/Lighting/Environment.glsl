// 物理光照角色按同一 NoV 与粗糙度取 DFG. 直接镜面获得单独的补偿,
// IBL 镜面使用已经乘过补偿的方向反照率, 两者均在外层施加光源尺度和曝光.
struct LightingDfgTerms
{
    SpecularCompensation directSpecular;
    vec3 environmentSpecular;
    vec3 indirectDiffuseBudget;
#ifdef PHYSARA_CLEARCOAT
    float coatEnvironment;
#endif
};

// 两个物理变体以 NoV 和过滤后 p 查同一 DFG, R=B, G=A+B. IOR/specular 显式保留 F90.
LightingDfgTerms PrepareLightingDfg(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection)
{
    float noV = CoreNormalViewCosine(geometry.shadingNormal, viewDirection);
    vec2 dfg = textureLod(sampler2D(dfgImage, environmentSampler), vec2(noV, pixel.perceptualRoughness), 0.0).rg;
    LightingDfgTerms terms;
    terms.directSpecular = UnitSpecularCompensation;
    if (UsesIorSpecular(pixel))
    {
        float a = dfg.g - dfg.r;
        float compensation = 1.0 / dfg.g - 1.0;
        // 介质同时改变 F0 和 F90. 强度只在单次反照率中计入一次.
        vec3 dielectric = (a * pixel.dielectricF0 + dfg.r) * pixel.specularWeight;
        vec3 metal = a * pixel.baseColor + dfg.r;
        terms.directSpecular.base = 1.0 + pixel.dielectricF0 * compensation;
        terms.directSpecular.metal = 1.0 + pixel.baseColor * compensation;
        dielectric *= terms.directSpecular.base;
        metal *= terms.directSpecular.metal;
        terms.environmentSpecular = mix(dielectric, metal, pixel.metallic);
        terms.indirectDiffuseBudget = vec3(max(1.0 - MaxReflectance(dielectric), 0.0));
    }
    else
    {
        CoreDfgTerms core = EvaluateCoreDfg(pixel, dfg);
        terms.directSpecular.base = core.specularCompensation;
        terms.environmentSpecular = core.directionalAlbedo * core.specularCompensation;
        terms.indirectDiffuseBudget = core.indirectDiffuseBudget;
    }
#ifdef PHYSARA_CLEARCOAT
    terms.coatEnvironment = 0.0;
    if (pixel.coatWeight > 0.0 && CoatFacesView(geometry, viewDirection))
    {
        float coatNoV = CoreNormalViewCosine(geometry.coatNormal, viewDirection);
        // Fresnel 已进入视向层权重, 因此这里查单位 Fresnel 的方向反照率 A+B.
        float coatDfg = textureLod(sampler2D(dfgImage, environmentSampler),
                                   vec2(coatNoV, pixel.coatRoughness), 0.0).g;
        terms.directSpecular.coat = 1.0 + CoatF0 * (1.0 / coatDfg - 1.0);
        terms.coatEnvironment = pixel.coatWeight * coatDfg * terms.directSpecular.coat;
    }
#endif
    return terms;
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

// 基底与清漆各按自己的粗糙度查 GGX 立方图. sheen 在 EvaluateIbl 中使用独立的 Charlie 立方图.
vec3 SampleEnvironmentReflection(vec3 normal, vec3 viewDirection, float alpha, float p)
{
    vec3 reflected = mix(reflect(-viewDirection, normal), normal, alpha * alpha);
    vec3 direction = environment.environmentFromWorld * UnitOr(reflected, normal);
    float lod = environment.maxMip * p * (2.0 - p);
    return textureLod(samplerCube(reflectionImage, environmentSampler), direction, lod).rgb;
}

// 材质遮蔽可共用, 视向余弦和微表面 alpha 取自当前反射波瓣.
float EnvironmentSpecularAo(float noV, float ao, float alpha)
{
    return clamp(pow(noV + ao, exp2(-16.0 * alpha - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// 漫反射与镜面共享环境逆旋转, 只将 AO 应用于间接分量.
vec3 EvaluateIbl(PixelParams pixel, SurfaceGeometry geometry, vec3 viewDirection, float ao, LightingDfgTerms terms,
                 float reflectionConfidence, vec3 preExposedReflection)
{
    vec3 normal = geometry.shadingNormal;
    float noV = CoreNormalViewCosine(normal, viewDirection);
    vec3 diffuse = EvaluateEnvironmentSh(environment.environmentFromWorld * normal);
    vec3 albedo = UsesIorSpecular(pixel) ? pixel.baseColor : pixel.diffuseColor;
    // IOR/specular 基底使用完整介质的 AO, 漫反射最后按金属度混合.
    vec3 diffuseFactor = albedo * terms.indirectDiffuseBudget * DiffuseVisibility(ao, albedo);
    if (UsesIorSpecular(pixel))
    {
        diffuseFactor *= 1.0 - pixel.metallic;
    }
    vec3 specular = SampleEnvironmentReflection(normal, viewDirection, pixel.alpha, pixel.perceptualRoughness);
    float specularAo = EnvironmentSpecularAo(noV, ao, pixel.alpha);
    vec3 specularFactor = terms.environmentSpecular * specularAo;
    float coatTransmission = 1.0;
#ifdef PHYSARA_CLEARCOAT
    // 先用层透过率衰减基底波瓣, 使其在预曝光后进入工作色限幅时仍保留可表示的亮区.
    coatTransmission = 1.0 - pixel.coatWeight;
#endif
    float lowerTransmission = coatTransmission;
#ifdef PHYSARA_SHEEN
    lowerTransmission *= SheenBaseTransmission(pixel);
#endif
    diffuseFactor *= lowerTransmission;
    specularFactor *= lowerTransmission;

    ivec3 exponent = ivec3(environment.radianceExponent);
    // 屏幕反射已乘置信度与预曝光. 两路镜面使用同一核心波瓣权重, 环境标定只影响环境输入.
    vec3 reflectedLight = ScaleEnvironmentLight(specular, specularFactor, environment.radianceMantissa, exponent) *
                          (1.0 - reflectionConfidence) + preExposedReflection * specularFactor;
    vec3 base = ScaleEnvironmentLight(diffuse, diffuseFactor, environment.radianceMantissa,
                                      exponent + environment.shExponent) + reflectedLight;
#ifdef PHYSARA_SHEEN
    if (pixel.sheenAlbedo > 0.0)
    {
        vec3 direction = environment.environmentFromWorld * reflect(-viewDirection, normal);
        float p = (pixel.sheenRoughness - MinPerceptualRoughness) / (1.0 - MinPerceptualRoughness);
        float lod = p * environment.sheenMaxMip;
        vec3 sheen = textureLod(samplerCube(sheenReflectionImage, environmentSampler), direction, lod).rgb;
        float visibility = EnvironmentSpecularAo(noV, ao, pixel.sheenAlpha);
        vec3 factor = pixel.sheenColor * (pixel.sheenAlbedo * visibility * coatTransmission);
        base += ScaleEnvironmentLight(sheen, factor, environment.radianceMantissa, exponent);
    }
#endif
#ifdef PHYSARA_CLEARCOAT
    if (terms.coatEnvironment > 0.0)
    {
        float coatNoV = CoreNormalViewCosine(geometry.coatNormal, viewDirection);
        vec3 coating = SampleEnvironmentReflection(geometry.coatNormal, viewDirection,
                                                   pixel.coatAlpha, pixel.coatRoughness);
        float visibility = EnvironmentSpecularAo(coatNoV, ao, pixel.coatAlpha);
        base += ScaleEnvironmentLight(coating, vec3(terms.coatEnvironment * visibility),
                                      environment.radianceMantissa, exponent);
    }
#endif
    return base;
}
#endif
