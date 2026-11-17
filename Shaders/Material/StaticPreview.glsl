// 标准材质的工作色预览: 基色, 世界法线, AO/roughness/metallic 或自发光, 尚不计算物理照明.
// 各模式复用共享取样/几何入口, 只读取其所需通道; 输出交给 Forward 的工作颜色附件.
#include <Source/Geometry/SurfaceFrame.glsl>
#include <Source/Material/StaticInputs.glsl>

// 调用前已用 baseColor.a 完成覆盖, baseColor 为同片元 SampleBaseColor 的结果.
// 模式值与 Renderer::PreviewMode 一致. Unlit 直接返回标准基色, 不受预览模式或其它贴图影响.
// 返回线性工作色或法线/通道的可视化数值; Material Channels 保留未经 BRDF 过滤的原始 p.
vec3 EvaluatePreview(vec4 baseColor)
{
    if (material.unlit != 0u)
    {
        return baseColor.rgb;
    }
    if (perView.mode == 1u)
    {
        bool mapped = HasMaterialTexture(2);
        vec3 tangentNormal = mapped ? SampleTangentNormal() : vec3(0.0, 0.0, 1.0);
        SurfaceGeometry geometry = PrepareSurfaceGeometry(tangentNormal, mapped);
        return 0.5 * geometry.shadingNormal + 0.5;
    }
    if (perView.mode == 2u)
    {
        MaterialInputs inputs = SampleMaterialInputs(baseColor);
        return vec3(inputs.occlusion, inputs.perceptualRoughness, inputs.metallic);
    }
    if (perView.mode == 3u)
    {
        // 保留超白的线性值, 上限是工作附件 FP16 的最大有限数.
        return min(SampleEmissive(), vec3(65504.0));
    }
    return baseColor.rgb;
}
