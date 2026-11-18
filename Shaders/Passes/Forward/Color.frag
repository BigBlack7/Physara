#version 450 core
#extension GL_GOOGLE_include_directive : require
// 静态 Forward 的颜色入口. 受光材质在 Direct Lighting 输出预曝光工作色, 其余模式输出材质诊断值.
// 两条路径共用基色与 MASK 覆盖, 后继显示 Pass 从 RGBA16F 读取工作色.
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticPreview.glsl>
#include <Source/Lighting/Punctual.glsl>

// 受光分支在本 Draw 内一致. 基色, 贴图梯度及几何粗糙度导数均在 MASK 覆盖分歧前取得.
void main()
{
    vec4 baseColor = SampleBaseColor();
    // 模式值与 Renderer::PreviewMode 一致. Unlit 仍沿标准基色路径.
    if (perView.mode == 4u && material.unlit == 0u)
    {
        bool mapped = HasMaterialTexture(2);
        vec3 tangentNormal = mapped ? SampleTangentNormal() : vec3(0.0, 0.0, 1.0);
        SurfaceGeometry geometry = PrepareSurfaceGeometry(tangentNormal, mapped);
        float kernel = ComputeGeometricRoughnessKernel(geometry.geometricNormal);
        MaterialInputs inputs = SampleMaterialInputs(baseColor);
        vec3 emission = SampleEmissive();
        ApplyCoverage(baseColor.a);

        // 保留的片元只构造一份表面参数. 正交相机共用反视线方向, 透视相机从当前片元指向眼点.
        PixelParams pixel = BuildPixelParams(inputs, kernel);
        // 基点按 Draw 平坦传递, 相对偏移的高低位分别插值. 同一表面位置供观察向量和每盏局部灯使用.
        LightingSurface surface = LightingSurface(vLightingAnchor, LightingPosition(vLightingOffsetHigh,
                                                                                     vLightingOffsetLow));
        LightingPosition eye = LightingPosition(perView.viewer, perView.viewerLow);
        // 透视分支只取眼点与表面之差的方向. 三项同时缩小到四分之一, 归一化后比例抵消.
        vec3 viewDirection = perView.orthographic != 0u ? perView.viewer :
            UnitOr(LightingDelta(eye, surface, 0.25), geometry.shadingNormal);
        outColor = vec4(EvaluateLighting(pixel, geometry.shadingNormal, viewDirection, surface, emission), 1.0);
        return;
    }
    ApplyCoverage(baseColor.a);
    outColor = vec4(EvaluatePreview(baseColor), 1.0);
}
