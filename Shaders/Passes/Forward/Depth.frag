#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Fragment.glsl>
#include <Source/Material/StaticCoverage.glsl>

// 深度附件由光栅器写入. 只有 MASK 需要进行基色 alpha 取样.
void main()
{
    MaterialUvGradients gradients = CaptureMaterialUvGradients();
    if (PHYSARA_FORWARD_MATERIAL.masked != 0u)
    {
        ApplyCoverage(SampleBaseColor(gradients).a);
    }
}
