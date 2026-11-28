// 从 StaticForward 生成接口的有效材质行和 UV 取样, 提供标准通道, 切线法线及线性自发光.
// 包含共享覆盖入口; 各槽使用 CPU 已验证的 UV 集, 仿射和颜色格式, 无槽时按标准默认值求值.
#include <Source/Common/UnitDirection.glsl>
#include <Source/Material/MaterialInputs.glsl>
#include <Source/Material/StaticCoverage.glsl>

// 以线性纹理 G/B 及材质因子取得未过滤的感知粗糙度和金属度. 缺席纹理的乘数为 1.
vec2 SampleRoughnessMetallic(MaterialUvGradients gradients)
{
    vec2 roughnessMetallic = vec2(1.0);
    if (HasMaterialTexture(1))
    {
        MaterialUvSample sampleUv = MaterialUv(1, gradients);
        roughnessMetallic = textureGrad(sampler2D(metallicRoughnessImage, metallicRoughnessSampler),
                                        sampleUv.uv, sampleUv.dx, sampleUv.dy).gb;
    }
    return roughnessMetallic * PHYSARA_FORWARD_MATERIAL.surface.yx;
}

// baseColor 与覆盖共用. AO 用线性纹理 R 及 strength 求值. 调用者按需处理法线、自发光及过滤.
MaterialInputs SampleMaterialInputs(vec4 baseColor, MaterialUvGradients gradients)
{
    vec2 roughnessMetallic = SampleRoughnessMetallic(gradients);
    float occlusion = 1.0;
    if (HasMaterialTexture(3))
    {
        MaterialUvSample sampleUv = MaterialUv(3, gradients);
        occlusion = textureGrad(sampler2D(occlusionImage, occlusionSampler), sampleUv.uv, sampleUv.dx, sampleUv.dy).r;
    }
#ifdef PHYSARA_HAS_SPECULAR_INPUTS
    // 两个槽独立选择 UV 和 sampler. 强度取线性 A, 镜面色的 RGB 由 sRGB 纹理格式解码.
    const int strengthSlot = int(SpecularTextureIndex);
    const int colorSlot = int(SpecularColorTextureIndex);
    vec4 specular = PHYSARA_FORWARD_MATERIAL.specular;
#ifdef PHYSARA_CLEARCOAT
    bool iorSpecularBase = PHYSARA_FORWARD_MATERIAL.clearcoat.w != 0.0;
#else
    const bool iorSpecularBase = true;
#endif
    if (iorSpecularBase && HasMaterialTexture(strengthSlot))
    {
        MaterialUvSample sampleUv = MaterialUv(strengthSlot, gradients);
        specular.w *= textureGrad(sampler2D(specularImage, specularSampler),
                                  sampleUv.uv, sampleUv.dx, sampleUv.dy).a;
    }
    if (iorSpecularBase && HasMaterialTexture(colorSlot))
    {
        MaterialUvSample sampleUv = MaterialUv(colorSlot, gradients);
        specular.rgb *= textureGrad(sampler2D(specularColorImage, specularColorSampler),
                                    sampleUv.uv, sampleUv.dx, sampleUv.dy).rgb;
    }
#endif
#ifdef PHYSARA_CLEARCOAT
    float coatStrength = PHYSARA_FORWARD_MATERIAL.clearcoat.x;
    float coatRoughness = PHYSARA_FORWARD_MATERIAL.clearcoat.y;
    if (coatStrength > 0.0 && HasMaterialTexture(int(ClearcoatTextureIndex)))
    {
        MaterialUvSample uv = MaterialUv(int(ClearcoatTextureIndex), gradients);
        coatStrength *= textureGrad(sampler2D(clearcoatImage, clearcoatSampler), uv.uv, uv.dx, uv.dy).r;
    }
    if (coatStrength > 0.0 && HasMaterialTexture(int(ClearcoatRoughnessTextureIndex)))
    {
        MaterialUvSample uv = MaterialUv(int(ClearcoatRoughnessTextureIndex), gradients);
        coatRoughness *= textureGrad(sampler2D(clearcoatRoughnessImage, clearcoatRoughnessSampler),
                                    uv.uv, uv.dx, uv.dy).g;
    }
#endif
    return MaterialInputs(baseColor, roughnessMetallic.x, roughnessMetallic.y,
                          mix(1.0, occlusion, PHYSARA_FORWARD_MATERIAL.surface.z)
#ifdef PHYSARA_HAS_SPECULAR_INPUTS
                          , PHYSARA_FORWARD_MATERIAL.ior, specular.w, specular.rgb
#endif
#ifdef PHYSARA_CLEARCOAT
                          , coatStrength, coatRoughness, iorSpecularBase
#endif
                          );
}

#ifdef PHYSARA_CLEARCOAT
// 清漆法线使用自己的线性图像, XY scale 和 UV 梯度.
vec3 SampleCoatTangentNormal(MaterialUvGradients gradients)
{
    MaterialUvSample uv = MaterialUv(int(ClearcoatNormalTextureIndex), gradients);
    vec3 normal = textureGrad(sampler2D(clearcoatNormalImage, clearcoatNormalSampler),
                              uv.uv, uv.dx, uv.dy).rgb * 2.0 - 1.0;
    normal.xy *= PHYSARA_FORWARD_MATERIAL.clearcoat.z;
    return UnitOr(normal, vec3(0.0, 0.0, 1.0));
}
#endif

// 调用前须确认 HasMaterialTexture(2). 线性 RGB 从 [0, 1] 解码为切线空间方向, 只缩放 XY.
// 返回单位方向供 PrepareSurfaceGeometry 使用; 解码缩放后全零则回退切线空间 +Z.
vec3 SampleTangentNormal(MaterialUvGradients gradients)
{
    MaterialUvSample sampleUv = MaterialUv(2, gradients);
    vec3 normal =
        textureGrad(sampler2D(normalImage, normalSampler), sampleUv.uv, sampleUv.dx, sampleUv.dy).rgb * 2.0 - 1.0;
    normal.xy *= PHYSARA_FORWARD_MATERIAL.surface.w;
    return UnitOr(normal, vec3(0.0, 0.0, 1.0));
}

// 返回未预曝光的线性 emissive texture * factor * strength, 缺席纹理按白色处理.
// SRGB 纹理格式负责采样时的 RGB 解码, 此处保留超白值; FP16 写出限幅由消费者承担.
vec3 SampleEmissive(MaterialUvGradients gradients)
{
    vec3 texel = vec3(1.0);
    if (HasMaterialTexture(4))
    {
        MaterialUvSample sampleUv = MaterialUv(4, gradients);
        texel = textureGrad(sampler2D(emissiveImage, emissiveSampler), sampleUv.uv, sampleUv.dx, sampleUv.dy).rgb;
    }
    return (texel * PHYSARA_FORWARD_MATERIAL.emissive.rgb) * PHYSARA_FORWARD_MATERIAL.emissive.w;
}
