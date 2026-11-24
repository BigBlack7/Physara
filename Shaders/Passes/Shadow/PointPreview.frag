#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

// 点光程序需要同一灯的完整六面深度组.
#define PHYSARA_POINT_SHADOW_PREVIEW
#include <Source/Passes/Shadow/LocalPreview.glsl>
