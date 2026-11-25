#version 450 core
#extension GL_GOOGLE_include_directive : require
// 基础颜色角色共用材质和光照入口, 逐灯可见性为 1.
#include <Source/Passes/Forward/Color.glsl>
