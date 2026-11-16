// 标准材质的工作色预览: 基色, 世界法线, AO/roughness/metallic 或自发光, 尚不计算物理照明.
#include <Source/Geometry/SurfaceFrame.glsl>

// 模式值与 Renderer::PreviewMode 一致. Unlit 的输出只由标准基色决定.
vec3 EvaluatePreview(vec3 baseColor)
{
    if (material.unlit != 0u)
    {
        return baseColor;
    }
    if (perView.mode == 1u)
    {
        bool mapped = HasMaterialTexture(2);
        vec3 texel = mapped ? texture(sampler2D(normalImage, normalSampler), MaterialUv(2)).rgb : vec3(0.0);
        return 0.5 * EvaluateSurfaceNormal(texel, mapped) + 0.5;
    }
    if (perView.mode == 2u)
    {
        vec2 mr = HasMaterialTexture(1) ? texture(sampler2D(metallicRoughnessImage, metallicRoughnessSampler),
                                                  MaterialUv(1)).gb : vec2(1.0);
        float ao = HasMaterialTexture(3) ? texture(sampler2D(occlusionImage, occlusionSampler), MaterialUv(3)).r
                                         : 1.0;
        return vec3(mix(1.0, ao, material.surface.z), mr.x * material.surface.y, mr.y * material.surface.x);
    }
    if (perView.mode == 3u)
    {
        vec3 texel = HasMaterialTexture(4) ? texture(sampler2D(emissiveImage, emissiveSampler), MaterialUv(4)).rgb
                                          : vec3(1.0);
        // 保留超白的线性值, 上限是工作附件 FP16 的最大有限数.
        return min((texel * material.emissive.rgb) * material.emissive.w, vec3(65504.0));
    }
    return baseColor;
}
