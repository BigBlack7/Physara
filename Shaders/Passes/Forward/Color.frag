#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticCoverage.glsl>
#include <Source/Material/StaticPreview.glsl>

// 工作颜色是单位倍率下的场景预览, 覆盖已由同一 MASK 合同决定.
void main()
{
    vec4 baseColor = SampleBaseColor();
    ApplyCoverage(baseColor.a);
    outColor = vec4(EvaluatePreview(baseColor.rgb), 1.0);
}
