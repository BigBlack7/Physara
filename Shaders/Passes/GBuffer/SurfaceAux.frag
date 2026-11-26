#version 450 core
#extension GL_GOOGLE_include_directive : require

#include <Interface/Fragment.glsl>
#include <Source/Geometry/SurfaceFrame.glsl>
#include <Source/Material/StaticInputs.glsl>
#include <Source/Material/CoreGBuffer.glsl>

// 辅助表面只写 G1 和 G3, D32 取共享顶点的光栅深度. 在 MASK 的逐片元丢弃前求 UV 梯度与粗糙度核,
// 使存活片元即使位于遮罩边界也能使用同一像素尺度的过滤结果.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    float kernel = ComputeGeometricRoughnessKernel(GeometricNormal());
    if (PHYSARA_FORWARD_MATERIAL.masked != 0u)
    {
        ApplyCoverage(SampleBaseColor(gradients).a);
    }
    bool mapped = HasMaterialTexture(2);
    vec3 tangentNormal = mapped ? SampleTangentNormal(gradients) : vec3(0.0, 0.0, 1.0);
    SurfaceGeometry geometry = PrepareSurfaceGeometry(tangentNormal, mapped);
    float roughness = FilterCoreRoughness(SampleRoughnessMetallic(gradients).x, kernel);
    // 辅助配方把 G1 绑定在位置 0. 整个 G3 被实际覆盖者替换, 因而清除核心资格.
    outColor = vec4(EncodeCoreNormal(geometry.shadingNormal), roughness, 0.0);
    outFlags = PHYSARA_FORWARD_MATERIAL.unlit != 0u ? SurfaceUnlit : SurfaceForwardExtension;
}
