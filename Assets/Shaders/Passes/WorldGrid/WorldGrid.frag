#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/FrameUniforms.glsl"

layout(location = 0)in vec3 inNearPoint;
layout(location = 1)in vec3 inFarPoint;

layout(std140, binding = PHYSARA_BINDING_WORLD_GRID_SETTINGS)uniform WorldGridSettingsBuffer
{
    vec4 uSpacingFade;
    vec4 uMinorColor;
    vec4 uMajorColor;
    vec4 uXAxisColor;
    vec4 uZAxisColor;
};

layout(location = 0)out vec4 outColor;

float GridLine(vec2 coordinate, float width)
{
    vec2 derivative = max(fwidth(coordinate), vec2(PHYSARA_EPSILON));
    vec2 distanceToLine = abs(fract(coordinate - 0.5) - 0.5) / derivative;
    return 1.0 - min(min(distanceToLine.x, distanceToLine.y) / width, 1.0);
}

float AxisLine(float coordinate)
{
    float derivative = max(fwidth(coordinate), PHYSARA_EPSILON);
    return 1.0 - smoothstep(derivative * 0.5, derivative * 1.5, abs(coordinate));
}

void main()
{
    vec3 ray = inFarPoint - inNearPoint;
    if (abs(ray.y) <= PHYSARA_EPSILON)
    {
        discard;
    }

    float distanceAlongRay = -inNearPoint.y / ray.y;
    if (distanceAlongRay <= 0.0 || distanceAlongRay >= 1.0)
    {
        discard;
    }

    vec3 worldPosition = inNearPoint + ray * distanceAlongRay;
    vec4 clipPosition = uFrame.camera.viewProjection * vec4(worldPosition, 1.0);
    float ndcDepth = clipPosition.z / clipPosition.w;
    gl_FragDepth = clamp(ndcDepth * 0.5 + 0.5, 0.0, 1.0);

    float minorSpacing = max(uSpacingFade.x, 0.001);
    float majorSpacing = minorSpacing * max(uSpacingFade.y, 2.0);
    float minorLine = GridLine(worldPosition.xz / minorSpacing, 1.0);
    float majorLine = GridLine(worldPosition.xz / majorSpacing, 1.2);
    float xAxis = AxisLine(worldPosition.z);
    float zAxis = AxisLine(worldPosition.x);

    vec4 gridColor = mix(uMinorColor, uMajorColor, majorLine);
    float gridAlpha = max(minorLine * uMinorColor.a, majorLine * uMajorColor.a);
    gridColor.a = gridAlpha;
    gridColor = mix(gridColor, uXAxisColor, xAxis);
    gridColor = mix(gridColor, uZAxisColor, zAxis * (1.0 - xAxis));

    float cameraDistance = length(worldPosition - GetCameraPosition(uFrame.camera));
    float distanceFade = 1.0 - smoothstep(uSpacingFade.z, uSpacingFade.w, cameraDistance);
    float angleFade = smoothstep(0.015, 0.08, abs(normalize(ray).y));
    gridColor.a *= distanceFade * angleFade;
    if (gridColor.a <= 0.001)
    {
        discard;
    }
    outColor = gridColor;
}