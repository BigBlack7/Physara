#version 450 core
#extension GL_GOOGLE_include_directive : require

// 共用表面及颜色入口, 受光片元按 Froxel 选择局部灯行.
#define PHYSARA_FORWARD_FROXELS
#include <Source/Passes/Forward/Color.glsl>
