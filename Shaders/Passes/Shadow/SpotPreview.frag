#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

// 聚光程序只有一个活动的深度视图.
#include <Source/Passes/Shadow/LocalPreview.glsl>
