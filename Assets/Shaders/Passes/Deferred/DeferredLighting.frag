#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/ClusteredLighting.glsl"
#include "../../Includes/IBL.glsl"
#include "../../Includes/Packing.glsl"
#include "../../Includes/Shadowing.glsl"

layout(location = 0)in vec2 inUV;

layout(std430, binding = PHYSARA_BINDING_LIGHTS)readonly buffer LightBuffer
{
    uint uLightCount;
    uint uLightPadding0;
    uint uLightPadding1;
    uint uLightPadding2;
    LightData uLights[];
};

layout(binding = PHYSARA_BINDING_SCENE_DEPTH_TEXTURE)uniform sampler2D uSceneDepth;
layout(binding = PHYSARA_BINDING_GBUFFER_BASE_COLOR_TEXTURE)uniform sampler2D uGBufferBaseColor;
layout(binding = PHYSARA_BINDING_GBUFFER_NORMAL_TEXTURE)uniform sampler2D uGBufferNormal;
layout(binding = PHYSARA_BINDING_GBUFFER_MATERIAL_TEXTURE)uniform sampler2D uGBufferMaterial;
layout(binding = PHYSARA_BINDING_GBUFFER_EMISSIVE_TEXTURE)uniform sampler2D uGBufferEmissive;

layout(location = 0)out vec4 outColor;

vec3 ReconstructWorldPosition(float depth)
{
    vec4 clip = vec4(inUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = uFrame.camera.inverseViewProjection * clip;
    return world.xyz / max(abs(world.w), PHYSARA_EPSILON) * (world.w < 0.0 ? -1.0 : 1.0);
}

PixelMaterial DecodeGBufferMaterial(vec4 baseColorAO, vec4 packedMaterial, vec3 emissive)
{
    PixelMaterial material;
    material.baseColor = vec4(baseColorAO.rgb, 1.0);
    material.metallic = Saturate(packedMaterial.y);
    material.perceptualRoughness = clamp(packedMaterial.x, 0.045, 1.0);
    material.roughness = material.perceptualRoughness * material.perceptualRoughness;
    material.reflectance = Saturate(packedMaterial.z);
    material.ambientOcclusion = Saturate(baseColorAO.a);
    material.alphaCutoff = 0.0;
    material.normalScale = 1.0;
    material.doubleSided = false;
    material.energyCompensation = vec3(1.0);
    material.shadingModel = PHYSARA_SHADING_MODEL_LIT;
    material.alphaMode = PHYSARA_ALPHA_OPAQUE;
    material.diffuseColor = (1.0 - material.metallic) * material.baseColor.rgb;
    material.f0 = 0.16 * material.reflectance * material.reflectance * (1.0 - material.metallic) +
                  material.baseColor.rgb * material.metallic;
    material.f90 = vec3(Saturate(Luminance(material.f0) * 50.0));
    material.emissive = max(emissive, vec3(0.0));
    return material;
}

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
    float depth = texture(uSceneDepth, inUV).r;
    if (depth >= 0.999999)
    {
        discard;
    }

    vec4 baseColorAO = texture(uGBufferBaseColor, inUV);
    vec3 normal = UnpackNormalOctahedron(texture(uGBufferNormal, inUV).rg);
    vec4 packedMaterial = texture(uGBufferMaterial, inUV);
    vec3 emissive = texture(uGBufferEmissive, inUV).rgb;
    vec3 worldPosition = ReconstructWorldPosition(depth);
    uint debugView = uint(uFrame.debugParams.x + 0.5);
    if (debugView == 1u || debugView == 8u)
    {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    if (debugView == 5u)
    {
        outColor = vec4(CascadeDebugColor(worldPosition), 1.0);
        return;
    }
    if (debugView == 7u)
    {
        outColor = vec4(baseColorAO.rgb, 1.0);
        return;
    }
    if (debugView == 9u)
    {
        outColor = vec4(packedMaterial.rgb, 1.0);
        return;
    }
    if (debugView == 10u)
    {
        outColor = vec4(emissive * GetPreExposure(uFrame.camera), 1.0);
        return;
    }

    ClusterEntry cluster = GetFragmentCluster(worldPosition);
    if (debugView == 6u)
    {
        outColor = vec4(VisualizeFragmentCluster(worldPosition, cluster), 1.0);
        return;
    }

    PixelMaterial material = DecodeGBufferMaterial(baseColorAO, packedMaterial, emissive);
    ShadingContext context;
    context.worldPosition = worldPosition;
    context.normal = normal;
    context.view = normalize(GetCameraPosition(uFrame.camera) - worldPosition);
    material.energyCompensation = ComputeIBLEnergyCompensation(material, normal, context.view);

    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
    for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
    {
        if (uint(uLights[lightIndex].directionType.w + 0.5) != PHYSARA_LIGHT_DIRECTIONAL)
        {
            continue;
        }
        float shadow = SampleShadow(
            worldPosition,
            normal,
            uLights[lightIndex],
            lightIndex,
            packedMaterial.w > 0.5);
        color += EvaluateLight(material, context, uLights[lightIndex]) * shadow;
    }
    for (uint clusterLight = 0u; clusterLight < cluster.count; ++clusterLight)
    {
        uint lightIndex = uClusterLightIndices[cluster.offset + clusterLight];
        if (lightIndex < lightCount)
        {
            color += EvaluateLight(material, context, uLights[lightIndex]);
        }
    }
    color += EvaluateIBL(material, normal, context.view);
    color += material.emissive * GetPreExposure(uFrame.camera);
    outColor = vec4(color, 1.0);
}