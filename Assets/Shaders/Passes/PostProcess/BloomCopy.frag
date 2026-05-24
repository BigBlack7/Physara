#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Common.glsl"

layout(location = 0)in vec2 inUV;
layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(location = 0)out vec4 outColor;

void main()
{
    outColor = vec4(max(texture(uSceneColor, inUV).rgb, vec3(0.0)), 1.0);
}