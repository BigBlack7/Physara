#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Lighting.glsl"

layout(location = 0)in vec3 inWorldPosition;
layout(location = 1)in vec3 inWorldNormal;
layout(location = 2)in vec4 inWorldTangent;
layout(location = 3)in vec2 inTexCoord0;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std140, binding = PHYSARA_BINDING_MATERIALS)uniform MaterialBuffer
{
    MaterialData uMaterial;
};

layout(std430, binding = PHYSARA_BINDING_LIGHTS)readonly buffer LightBuffer
{
    uint uLightCount;
    uint uLightPadding0;
    uint uLightPadding1;
    uint uLightPadding2;
    LightData uLights[];
};

layout(location = 0)out vec4 outColor;

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterial);
    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }
    
    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        outColor = vec4(material.emissive, material.baseColor.a);
        return;
    }
    
    ShadingContext context;
    context.worldPosition = inWorldPosition;
    context.normal = normalize(inWorldNormal);
    context.view = normalize(GetCameraPosition(uCamera) - inWorldPosition);
    
    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
    for(uint i = 0u; i < lightCount; ++ i)
    {
        color += EvaluateLight(material, context, uLights[i]);
    }
    color += material.emissive;
    outColor = vec4(color, material.baseColor.a);
}