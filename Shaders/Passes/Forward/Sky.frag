#version 450 core
#extension GL_GOOGLE_include_directive : require
#include <Interface/Fragment.glsl>
#include <Source/Lighting/WorkingLight.glsl>

// 查询未卷积的环境层. 只写预曝光 RGB, 不透明覆盖由附件的目的 alpha 决定.
void main()
{
    vec3 direction = skyView.environmentFromClip * vec3(vClip, 1.0);
    vec3 radiance = textureLod(samplerCube(skyImage, skySampler), direction, 0.0).rgb;
    outColor = vec4(ScaleEnvironmentLight(radiance, vec3(1.0), skyView.radianceMantissa, ivec3(skyView.radianceExponent)),
                    1.0);
}
