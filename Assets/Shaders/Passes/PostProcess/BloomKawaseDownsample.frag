#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Math.glsl"

layout(location = 0)in vec2 inUV;

layout(std140, binding = PHYSARA_BINDING_POST_PROCESS_SETTINGS)uniform PostProcessSettingsBuffer
{
    vec4 uBloomParams;
    vec4 uFlags;
    vec4 uExposureParams;
    vec4 uAAParams;
};

layout(binding = PHYSARA_BINDING_SCENE_COLOR_TEXTURE)uniform sampler2D uSceneColor;
layout(location = 0)out vec4 outColor;

void main()
{
    vec2 texel = 1.0 / max(vec2(textureSize(uSceneColor, 0)), vec2(1.0));
    float offset = max(uBloomParams.w, 0.5);
    vec3 color = texture(uSceneColor, inUV).rgb * 0.5;
    color += texture(uSceneColor, inUV + vec2(offset, offset) * texel).rgb * 0.125;
    color += texture(uSceneColor, inUV + vec2(-offset, offset) * texel).rgb * 0.125;
    color += texture(uSceneColor, inUV + vec2(offset, -offset) * texel).rgb * 0.125;
    color += texture(uSceneColor, inUV + vec2(-offset, -offset) * texel).rgb * 0.125;
    outColor = vec4(clamp(color, vec3(0.0), vec3(60000.0)), 1.0);
}