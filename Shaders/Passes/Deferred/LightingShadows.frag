#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define PHYSARA_FORWARD_FROXELS
#define PHYSARA_FORWARD_SHADOWS
#include <Source/Passes/Deferred/Lighting.glsl>
