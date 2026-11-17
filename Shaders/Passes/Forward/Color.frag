#version 450 core
#extension GL_GOOGLE_include_directive : require
// 静态 Forward 的薄预览入口, 消费生成 ABI 并输出 RGBA16F 工作色; 显示转换由后继 Pass 完成.
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticPreview.glsl>

// 基色只取样一次, 先执行与 Depth 共用的 MASK 覆盖, 再选择预览响应; 保留片元的输出 alpha 为 1.
// 当前预览不使用几何粗糙度过滤; 物理着色接入时须在覆盖分歧前取得几何法线导数.
void main()
{
    vec4 baseColor = SampleBaseColor();
    ApplyCoverage(baseColor.a);
    outColor = vec4(EvaluatePreview(baseColor), 1.0);
}
