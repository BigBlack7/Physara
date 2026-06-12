#ifndef PHYSARA_FRAME_UNIFORMS_GLSL
#define PHYSARA_FRAME_UNIFORMS_GLSL

#include "Common.glsl"

layout(std140, binding = PHYSARA_BINDING_FRAME_UNIFORMS)uniform FrameUniformBuffer
{
    FrameUniforms uFrame;
};

#endif