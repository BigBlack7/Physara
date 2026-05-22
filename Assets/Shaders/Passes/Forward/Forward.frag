#version 460 core
#extension GL_ARB_shading_language_include : require
#include "../../Includes/Lighting.glsl"

layout(location = 0)in vec3 inWorldPosition;
layout(location = 1)in vec3 inWorldNormal;
layout(location = 2)in vec4 inWorldTangent;
layout(location = 3)in vec2 inTexCoord0;
layout(location = 4)in vec2 inTexCoord1;
layout(location = 5)flat in uint inMaterialIndex;

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

layout(std140, binding = PHYSARA_BINDING_RENDER_SETTINGS)uniform RenderSettingsBuffer
{
    vec4 uDebugParams;
};

layout(std140, binding = PHYSARA_BINDING_SHADOW)uniform ShadowBuffer
{
    ShadowData uShadow;
};

layout(binding = PHYSARA_BINDING_BASE_COLOR_TEXTURE)uniform sampler2D uBaseColorTexture;
layout(binding = PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE)uniform sampler2D uMetallicRoughnessTexture;
layout(binding = PHYSARA_BINDING_NORMAL_TEXTURE)uniform sampler2D uNormalTexture;
layout(binding = PHYSARA_BINDING_OCCLUSION_TEXTURE)uniform sampler2D uOcclusionTexture;
layout(binding = PHYSARA_BINDING_EMISSIVE_TEXTURE)uniform sampler2D uEmissiveTexture;
layout(binding = PHYSARA_BINDING_SHADOW_MAP)uniform sampler2DShadow uShadowMap;

layout(location = 0)out vec4 outColor;

vec3 SrgbToLinear(vec3 value)
{
    return pow(max(value, vec3(0.0)), vec3(2.2));
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
    if (inputs.flipNormalY)
    {
        tangentNormal.y = -tangentNormal.y;
    }
    tangentNormal.xy *= inputs.normalScale;
    return normalize(tbn * tangentNormal);
}

vec2 SelectTexCoord(uint texCoordSet)
{
    return texCoordSet == 1u ? inTexCoord1 : inTexCoord0;
}

float SampleShadowPCF3x3(vec3 worldPosition, vec3 normal, LightData light, uint lightIndex)
{
    if (uShadow.params.x < 0.5 || uint(uShadow.params.z + 0.5) != lightIndex)
    {
        return 1.0;
    }

    vec4 lightClip = uShadow.lightViewProjection * vec4(worldPosition, 1.0);
    vec3 projected = lightClip.xyz / max(lightClip.w, PHYSARA_EPSILON);
    vec3 shadowCoord = projected * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0 || shadowCoord.z > 1.0)
    {
        return 1.0;
    }

    vec3 lightToSurface = SafeNormalize(light.directionType.xyz);
    float NoL = Saturate(dot(normal, -lightToSurface));
    float receiverBiasScale = max(uShadow.controls.x, 0.0);
    float bias = max(light.shadowParams.y * receiverBiasScale * (1.0 - NoL), light.shadowParams.y * receiverBiasScale * 0.25);
    float texel = max(uShadow.params.w, 1.0 / max(uShadow.params.y, 1.0));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * texel;
            visibility += texture(uShadowMap, vec3(shadowCoord.xy + offset, shadowCoord.z - bias));
        }
    }
    return visibility / 9.0;
}

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    
    if (inputs.hasBaseColorTexture)
    {
        vec4 baseColorSample = texture(uBaseColorTexture, SelectTexCoord(inputs.baseColorTexCoord));
        inputs.baseColor *= vec4(SrgbToLinear(baseColorSample.rgb), baseColorSample.a);
    }
    if (inputs.hasMetallicRoughnessTexture)
    {
        vec4 mrSample = texture(uMetallicRoughnessTexture, SelectTexCoord(inputs.metallicRoughnessTexCoord));
        inputs.perceptualRoughness *= mrSample.g;
        inputs.metallic *= mrSample.b;
    }
    if (inputs.hasOcclusionTexture)
    {
        float occlusionSample = texture(uOcclusionTexture, SelectTexCoord(inputs.occlusionTexCoord)).r;
        inputs.ambientOcclusion *= occlusionSample;
    }
    if (inputs.hasEmissiveTexture)
    {
        vec3 emissiveSample = texture(uEmissiveTexture, SelectTexCoord(inputs.emissiveTexCoord)).rgb;
        inputs.emissiveColor *= SrgbToLinear(emissiveSample);
    }
    
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
    vec3 geometricNormal = normalize(inWorldNormal);
    if (inputs.doubleSided && ! gl_FrontFacing)
    {
        geometricNormal = -geometricNormal;
    }
    context.normal = ResolveWorldNormal(inputs, geometricNormal, inWorldTangent, SelectTexCoord(inputs.normalTexCoord));
    context.view = normalize(GetCameraPosition(uCamera) - inWorldPosition);

    uint debugView = uint(uDebugParams.x + 0.5);
    if (debugView == 1u)
    {
        outColor = vec4(context.normal * 0.5 + 0.5, material.baseColor.a);
        return;
    }
    
    vec3 color = vec3(0.0);
    uint lightCount = min(uLightCount, uint(PHYSARA_MAX_LIGHTS));
    for(uint i = 0u; i < lightCount; ++ i)
    {
        float shadowVisibility = SampleShadowPCF3x3(context.worldPosition, context.normal, uLights[i], i);
        color += EvaluateLight(material, context, uLights[i]) * shadowVisibility;
    }
    vec3 ambient = material.diffuseColor * material.ambientOcclusion * 0.04;
    color += ambient;
    color += material.emissive;
    outColor = vec4(color, material.baseColor.a);
}