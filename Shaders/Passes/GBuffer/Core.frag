#version 450 core
#extension GL_GOOGLE_include_directive : require

#include <Interface/Fragment.glsl>
#include <Source/Geometry/SurfaceFrame.glsl>
#include <Source/Material/StaticInputs.glsl>
#include <Source/Material/CoreGBuffer.glsl>
#include <Source/Lighting/WorkingLight.glsl>

// 仅核心批次进入本 Pass. 梯度在覆盖测试前取得, 幸存片元写完整的材质和资格.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    float kernel = ComputeGeometricRoughnessKernel(GeometricNormal());
    vec4 baseColor = SampleBaseColor(gradients);
    ApplyCoverage(baseColor.a);
    MaterialInputs material = SampleMaterialInputs(baseColor, gradients);
    bool mapped = HasMaterialTexture(2);
    vec3 tangentNormal = mapped ? SampleTangentNormal(gradients) : vec3(0.0, 0.0, 1.0);
    SurfaceGeometry geometry = PrepareSurfaceGeometry(tangentNormal, mapped);
    // G0 的 sRGB 附件只转换 RGB. alpha 留给线性材质 AO, 覆盖 alpha 已完成 MASK 测试.
    outColor = vec4(baseColor.rgb, material.occlusion);
    outNormalMaterial = vec4(EncodeCoreNormal(geometry.shadingNormal),
                             FilterCoreRoughness(material.perceptualRoughness, kernel), material.metallic);
    // 共用工作域缩放后按 G2 的 6/6/5 位尾数分别限幅, 单通道饱和不改变另外两路.
    outEmission = min(PreExposeEmission(SampleEmissive(gradients), perView.preExposure),
                      vec3(65024.0, 65024.0, 64512.0));
    outFlags = CoreDeferredEligible;
}
