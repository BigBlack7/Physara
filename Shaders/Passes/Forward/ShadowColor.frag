#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

// 在共用颜色入口启用逐灯查询. 实际组引用取自本次 View, 过滤档位由查询常量选择.
#define PHYSARA_FORWARD_SHADOWS
#include <Source/Passes/Forward/Color.glsl>
