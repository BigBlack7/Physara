#version 460 core
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Lighting.glsl"
#include "../../Includes/IBL.glsl"
#include "../../Includes/Shadowing.glsl"
#include "../../Includes/SurfaceMaterial.glsl"
#ifdef PHYSARA_CLUSTERED_LIGHTING
#include "../../Includes/ClusteredLighting.glsl"
#endif

layout(location = 0)in vec3 inWorldPosition;
layout(location = 1)in vec3 inWorldNormal;
layout(location = 2)in vec4 inWorldTangent;
layout(location = 3)in vec2 inTexCoord0;
layout(location = 4)in vec2 inTexCoord1;
layout(location = 5)flat in uint inMaterialIndex;
layout(location = 6)flat in uint inObjectFlags;

layout(std430, binding = PHYSARA_BINDING_LIGHTS)readonly buffer LightBuffer
{
    uint uLightCount;
    uint uLightPadding0;
    uint uLightPadding1;
    uint uLightPadding2;
    LightData uLights[];
};

layout(location = 0)out vec4 outColor;

vec3 CascadeDebugColor(vec3 worldPosition)
{
    int cascadeCount = clamp(int(uFrame.shadow.controls.x + 0.5), 1, PHYSARA_MAX_SHADOW_CASCADES);
    float viewDepth = max(-(uFrame.camera.view * vec4(worldPosition, 1.0)).z, 0.0);
    int cascadeIndex = SelectShadowCascade(viewDepth, cascadeCount);
    const vec3 colors[4] = vec3[4](
        vec3(1.0, 0.2, 0.2),
        vec3(0.2, 1.0, 0.2),
        vec3(0.2, 0.45, 1.0),
        vec3(1.0, 0.85, 0.2));
    return cascadeIndex >= 0 ? colors[cascadeIndex] : vec3(0.0);
}

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    ApplySurfaceTextures(inputs, inMaterialIndex, inTexCoord0, inTexCoord1);
    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }
    ShadingContext context;
    context.worldPosition = inWorldPosition;
    vec3 geometricNormal = normalize(inWorldNormal);
    if (inputs.doubleSided && !gl_FrontFacing)
    {
        geometricNormal = -geometricNormal;
    }
    context.normal = ResolveSurfaceWorldNormal(
        inputs,
        inMaterialIndex,
        geometricNormal,
        inWorldTangent,
        inTexCoord0,
        inTexCoord1);
    context.view = normalize(GetCameraPosition(uFrame.camera) - inWorldPosition);
    material.energyCompensation = ComputeIBLEnergyCompensation(material, context.normal, context.view);

    uint debugView = uint(uFrame.debugParams.x + 0.5);
    if (debugView == 1u)
    {
        outColor = vec4(context.normal * 0.5 + 0.5, material.baseColor.a);
        return;
    }
    if (debugView == 5u)
    {
        outColor = vec4(CascadeDebugColor(context.worldPosition), material.baseColor.a);
        return;
    }

#ifdef PHYSARA_CLUSTERED_LIGHTING
    ClusterEntry fragmentCluster = GetFragmentCluster(context.worldPosition);
    if (debugView == 6u)
    {
        outColor = vec4(
            VisualizeFragmentCluster(context.worldPosition, fragmentCluster),
            material.baseColor.a);
        return;
    }
#endif

    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        outColor = vec4(
            (material.baseColor.rgb + material.emissive) * GetPreExposure(uFrame.camera),
            material.baseColor.a);
        return;
    }

    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
#ifdef PHYSARA_CLUSTERED_LIGHTING
    for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
    {
        if (uint(uLights[lightIndex].directionType.w + 0.5) != PHYSARA_LIGHT_DIRECTIONAL)
        {
            continue;
        }
        float shadow = SampleShadow(
            context.worldPosition,
            context.normal,
            uLights[lightIndex],
            lightIndex,
            (inObjectFlags & PHYSARA_OBJECT_RECEIVE_SHADOW) != 0u);
        color += EvaluateLight(material, context, uLights[lightIndex]) * shadow;
    }
    for (uint clusterLight = 0u; clusterLight < fragmentCluster.count; ++clusterLight)
    {
        uint lightIndex = uClusterLightIndices[fragmentCluster.offset + clusterLight];
        if (lightIndex < lightCount)
        {
            color += EvaluateLight(material, context, uLights[lightIndex]);
        }
    }
#else
    for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
    {
        float shadow = SampleShadow(
            context.worldPosition,
            context.normal,
            uLights[lightIndex],
            lightIndex,
            (inObjectFlags & PHYSARA_OBJECT_RECEIVE_SHADOW) != 0u);
        color += EvaluateLight(material, context, uLights[lightIndex]) * shadow;
    }
#endif
    color += EvaluateIBL(material, context.normal, context.view);
    color += material.emissive * GetPreExposure(uFrame.camera);
    outColor = vec4(color, material.baseColor.a);
}