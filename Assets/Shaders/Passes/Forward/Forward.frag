#version 460 core
#extension GL_ARB_shading_language_include : require

#include "../../Includes/Lighting.glsl"

layout(location = 0)in vec3 inWorldPosition;
layout(location = 1)in vec3 inWorldNormal;
layout(location = 2)in vec4 inWorldTangent;
layout(location = 3)in vec2 inTexCoord0;
layout(location = 4)flat in uint inMaterialIndex;

layout(std140, binding = PHYSARA_BINDING_CAMERA)uniform CameraBuffer
{
    CameraData uCamera;
};

layout(std430, binding = PHYSARA_BINDING_MATERIALS)readonly buffer MaterialBuffer
{
    MaterialData uMaterials[];
};

layout(std430, binding = PHYSARA_BINDING_LIGHTS)readonly buffer LightBuffer
{
    uint uLightCount;
    uint uLightPadding0;
    uint uLightPadding1;
    uint uLightPadding2;
    LightData uLights[];
};

layout(binding = PHYSARA_BINDING_BASE_COLOR_TEXTURE)uniform sampler2D uBaseColorTexture;
layout(binding = PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE)uniform sampler2D uMetallicRoughnessTexture;
layout(binding = PHYSARA_BINDING_NORMAL_TEXTURE)uniform sampler2D uNormalTexture;
layout(binding = PHYSARA_BINDING_OCCLUSION_TEXTURE)uniform sampler2D uOcclusionTexture;
layout(binding = PHYSARA_BINDING_EMISSIVE_TEXTURE)uniform sampler2D uEmissiveTexture;

layout(location = 0)out vec4 outColor;

vec3 SrgbToLinear(vec3 value)
{
    return pow(max(value, vec3(0.0)), vec3(2.2));
}

vec3 LinearToSrgb(vec3 value)
{
    return pow(max(value, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 TonemapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 ToViewportOutput(vec3 linearColor)
{
    vec3 exposed = linearColor * ExposureFromEV100(GetEV100(uCamera));
    return LinearToSrgb(TonemapACES(exposed));
}

vec3 ResolveWorldNormal(MaterialInputs inputs, vec3 geometricNormal, vec4 worldTangent, vec2 texCoord)
{
    if (!inputs.hasNormalTexture)
    {
        return normalize(geometricNormal);
    }

    vec3 n = normalize(geometricNormal);
    vec3 t = normalize(worldTangent.xyz - n * dot(n, worldTangent.xyz));
    vec3 b = normalize(cross(n, t) * worldTangent.w);
    mat3 tbn = mat3(t, b, n);
    vec3 tangentNormal = texture(uNormalTexture, texCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= inputs.normalScale;
    return normalize(tbn * tangentNormal);
}

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    vec4 baseColorSample = texture(uBaseColorTexture, inTexCoord0);
    vec4 mrSample = texture(uMetallicRoughnessTexture, inTexCoord0);
    float occlusionSample = texture(uOcclusionTexture, inTexCoord0).r;
    vec3 emissiveSample = texture(uEmissiveTexture, inTexCoord0).rgb;

    if (inputs.hasBaseColorTexture)
    {
        inputs.baseColor *= vec4(SrgbToLinear(baseColorSample.rgb), baseColorSample.a);
    }
    if (inputs.hasMetallicRoughnessTexture)
    {
        inputs.perceptualRoughness *= mrSample.g;
        inputs.metallic *= mrSample.b;
    }
    if (inputs.hasOcclusionTexture)
    {
        inputs.ambientOcclusion *= occlusionSample;
    }
    inputs.emissiveColor *= SrgbToLinear(emissiveSample);

    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }
    
    if (material.shadingModel == PHYSARA_SHADING_MODEL_UNLIT)
    {
        outColor = vec4(ToViewportOutput(material.emissive), material.baseColor.a);
        return;
    }
    
    ShadingContext context;
    context.worldPosition = inWorldPosition;
    context.normal = ResolveWorldNormal(inputs, inWorldNormal, inWorldTangent, inTexCoord0);
    context.view = normalize(GetCameraPosition(uCamera) - inWorldPosition);
    
    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
    for(uint i = 0u; i < lightCount; ++ i)
    {
        color += EvaluateLight(material, context, uLights[i]);
    }
    vec3 ambient = material.diffuseColor * material.ambientOcclusion * 0.04;
    color += ambient;
    color += material.emissive;
    outColor = vec4(ToViewportOutput(color), material.baseColor.a);
}