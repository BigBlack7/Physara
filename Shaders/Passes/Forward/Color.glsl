// 静态 Forward 的颜色入口. Direct Lighting 求单次散射直接光, Physical Lighting 用 DFG 补偿并按变体加入 IBL.
// 各模式共享基色与 MASK 覆盖. 存活片元写 alpha=1, 供后继天空混合识别几何覆盖.
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticPreview.glsl>
#include <Source/Lighting/Punctual.glsl>
#ifdef PHYSARA_PHYSICAL_LIGHTING
#include <Source/Lighting/Environment.glsl>
#endif

// 颜色与深度使用同一实例的材质行判断覆盖. 预览和受光路径共用覆盖前取得的梯度.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    // 模式在 View 内一致. 几何导数须在每实例的 Unlit 分歧和任何 discard 之前求出.
#ifdef PHYSARA_PHYSICAL_LIGHTING
    bool lightingMode = perView.mode == 5u;
#else
    bool lightingMode = perView.mode == 4u;
#endif
    float kernel = lightingMode ? ComputeGeometricRoughnessKernel(GeometricNormal()) : 0.0;
    vec4 baseColor = SampleBaseColor(gradients);
    if (lightingMode && PHYSARA_FORWARD_MATERIAL.unlit == 0u)
    {
        bool mapped = HasMaterialTexture(2);
        vec3 tangentNormal = mapped ? SampleTangentNormal(gradients) : vec3(0.0, 0.0, 1.0);
        SurfaceGeometry geometry = PrepareSurfaceGeometry(tangentNormal, mapped);
        MaterialInputs inputs = SampleMaterialInputs(baseColor, gradients);
        vec3 emission = SampleEmissive(gradients);
        ApplyCoverage(baseColor.a);

        // 保留的片元只构造一份表面参数. 正交相机共用反视线方向, 透视相机从当前片元指向眼点.
        PixelParams pixel = BuildPixelParams(inputs, kernel);
        // 基点按实例平坦传递, 相对偏移的高低位分别插值. 同一表面位置供观察向量和每盏局部灯使用.
        LightingSurface surface = LightingSurface(vLightingAnchor, LightingPosition(vLightingOffsetHigh,
                                                                                     vLightingOffsetLow));
        LightingPosition eye = LightingPosition(perView.viewer, perView.viewerLow);
        // 透视分支只取眼点与表面之差的方向. 三项同时缩小到四分之一, 归一化后比例抵消.
        vec3 viewDirection = perView.orthographic != 0u ? perView.viewer :
            UnitOr(LightingDelta(eye, surface, 0.25), geometry.shadingNormal);
        vec3 compensation = vec3(1.0);
#ifdef PHYSARA_PHYSICAL_LIGHTING
        CoreDfgTerms terms = PrepareCoreDfg(pixel, geometry.shadingNormal, viewDirection);
        compensation = terms.specularCompensation;
#endif
        vec3 result = EvaluateLighting(pixel, geometry, viewDirection, surface, emission, compensation);
#if PHYSARA_PHYSICAL_LIGHTING == 2
        result += EvaluateIbl(pixel, geometry.shadingNormal, viewDirection, inputs.occlusion, terms);
#endif
        outColor = vec4(min(result, vec3(MaxWorkingLight)), 1.0);
        return;
    }
    ApplyCoverage(baseColor.a);
    outColor = vec4(EvaluatePreview(baseColor, gradients), 1.0);
}
