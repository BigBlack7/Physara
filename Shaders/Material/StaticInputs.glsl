// 从 StaticForward 生成接口的有效材质行和 UV 取样, 提供标准通道, 切线法线及线性自发光.
// 包含共享覆盖入口; 各槽使用 CPU 已验证的 UV 集, 仿射和颜色格式, 无槽时按标准默认值求值.
#include <Source/Common/UnitDirection.glsl>
#include <Source/Material/MaterialInputs.glsl>
#include <Source/Material/StaticCoverage.glsl>

// baseColor 须为同片元 SampleBaseColor 的结果, 由调用方与覆盖共享, 此处不再次采样或乘因子.
// metallicRoughness 的 G/B 与 AO 的 R 均按线性数据解释; 缺席纹理为 1, AO 再按 strength 插值.
// 返回原始感知粗糙度, 不进行几何过滤或曝光; normal 与 emissive 由所需消费者单独取样.
MaterialInputs SampleMaterialInputs(vec4 baseColor, MaterialUvGradients gradients)
{
    vec2 roughnessMetallic = vec2(1.0);
    if (HasMaterialTexture(1))
    {
        MaterialUvSample sampleUv = MaterialUv(1, gradients);
        roughnessMetallic = textureGrad(sampler2D(metallicRoughnessImage, metallicRoughnessSampler),
                                        sampleUv.uv, sampleUv.dx, sampleUv.dy).gb;
    }
    float occlusion = 1.0;
    if (HasMaterialTexture(3))
    {
        MaterialUvSample sampleUv = MaterialUv(3, gradients);
        occlusion = textureGrad(sampler2D(occlusionImage, occlusionSampler), sampleUv.uv, sampleUv.dx, sampleUv.dy).r;
    }
    vec4 factors = PHYSARA_FORWARD_MATERIAL.surface;
    return MaterialInputs(baseColor, roughnessMetallic.x * factors.y, roughnessMetallic.y * factors.x,
                          mix(1.0, occlusion, factors.z));
}

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
