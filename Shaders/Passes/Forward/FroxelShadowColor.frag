#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

// 局部灯候选来自 Froxel, 阴影引用仍以同轮完整灯表的绝对行号查询.
#define PHYSARA_FORWARD_FROXELS
#define PHYSARA_FORWARD_SHADOWS
#include <Source/Passes/Forward/Color.glsl>
