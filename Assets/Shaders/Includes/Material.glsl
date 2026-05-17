#ifndef PHYSARA_MATERIAL_GLSL
#define PHYSARA_MATERIAL_GLSL

#include "BRDF.glsl"

struct MaterialData
{
    vec4 baseColor;
    vec4 emissiveColorLuminance;
    vec4 metallicRoughnessReflectanceAO;
    vec4 alphaNormalFlags;
    vec4 textureFlags;
    vec4 materialFlags;
};

struct MaterialInputs
{
    vec4 baseColor;
    float metallic;
    float perceptualRoughness;
    float reflectance;
    float ambientOcclusion;
    vec3 emissiveColor;
    float emissiveLuminance;
    float normalScale;
    float alphaCutoff;
    bool hasBaseColorTexture;
    bool hasMetallicRoughnessTexture;
    bool hasNormalTexture;
    bool hasOcclusionTexture;
    bool hasEmissiveTexture;
    bool doubleSided;
    uint shadingModel;
    uint alphaMode;
};

struct PixelMaterial
{
    vec4 baseColor;
    vec3 diffuseColor;
    vec3 f0;
    vec3 f90;
    vec3 emissive;
    float metallic;
    float perceptualRoughness;
    float roughness;
    float reflectance;
    float ambientOcclusion;
    float alphaCutoff;
    float normalScale;
    bool doubleSided;
    uint shadingModel;
    uint alphaMode;
};

MaterialInputs DefaultMaterialInputs()
{
    MaterialInputs material;
    material.baseColor = vec4(1.0);
    material.metallic = 0.0;
    material.perceptualRoughness = 0.5;
    material.reflectance = 0.5;
    material.ambientOcclusion = 1.0;
    material.emissiveColor = vec3(0.0);
    material.emissiveLuminance = 0.0;
    material.normalScale = 1.0;
    material.alphaCutoff = 0.5;
    material.hasBaseColorTexture = false;
    material.hasMetallicRoughnessTexture = false;
    material.hasNormalTexture = false;
    material.hasOcclusionTexture = false;
    material.hasEmissiveTexture = false;
    material.doubleSided = false;
    material.shadingModel = PHYSARA_SHADING_MODEL_LIT;
    material.alphaMode = PHYSARA_ALPHA_OPAQUE;
    return material;
}

MaterialInputs UnpackMaterialData(MaterialData data)
{
    MaterialInputs material;
    material.baseColor = data.baseColor;
    material.emissiveColor = data.emissiveColorLuminance.rgb;
    material.emissiveLuminance = data.emissiveColorLuminance.a;
    material.metallic = data.metallicRoughnessReflectanceAO.x;
    material.perceptualRoughness = data.metallicRoughnessReflectanceAO.y;
    material.reflectance = data.metallicRoughnessReflectanceAO.z;
    material.ambientOcclusion = data.metallicRoughnessReflectanceAO.w;
    material.alphaCutoff = data.alphaNormalFlags.x;
    material.normalScale = data.alphaNormalFlags.y;
    material.hasBaseColorTexture = data.textureFlags.x > 0.5;
    material.hasMetallicRoughnessTexture = data.textureFlags.y > 0.5;
    material.hasNormalTexture = data.textureFlags.z > 0.5;
    material.hasOcclusionTexture = data.textureFlags.w > 0.5;
    material.hasEmissiveTexture = data.materialFlags.y > 0.5;
    material.doubleSided = data.materialFlags.x > 0.5;
    material.shadingModel = uint(data.alphaNormalFlags.z);
    material.alphaMode = uint(data.alphaNormalFlags.w);
    return material;
}

PixelMaterial PrepareMaterial(MaterialInputs inputs)
{
    PixelMaterial material;
    material.baseColor = vec4(Saturate(inputs.baseColor.rgb), Saturate(inputs.baseColor.a));
    material.metallic = Saturate(inputs.metallic);
    material.perceptualRoughness = clamp(inputs.perceptualRoughness, 0.045, 1.0);
    material.roughness = material.perceptualRoughness * material.perceptualRoughness;
    material.reflectance = clamp(inputs.reflectance, 0.0, 1.0);
    material.ambientOcclusion = Saturate(inputs.ambientOcclusion);
    material.alphaCutoff = Saturate(inputs.alphaCutoff);
    material.normalScale = max(inputs.normalScale, 0.0);
    material.doubleSided = inputs.doubleSided;
    material.shadingModel = inputs.shadingModel;
    material.alphaMode = inputs.alphaMode;

    material.diffuseColor = (1.0 - material.metallic) * material.baseColor.rgb;
    material.f0 = 0.16 * material.reflectance * material.reflectance * (1.0 - material.metallic) +
                  material.baseColor.rgb * material.metallic;
    material.f90 = vec3(Saturate(Luminance(material.f0) * 50.0));
    float emissiveScale = inputs.emissiveLuminance > 0.0 ? inputs.emissiveLuminance : 1.0;
    material.emissive = max(inputs.emissiveColor, vec3(0.0)) * emissiveScale;
    return material;
}

bool ShouldDiscardMaterial(PixelMaterial material)
{
    return material.alphaMode == PHYSARA_ALPHA_MASK && material.baseColor.a < material.alphaCutoff;
}

BRDFInputs BuildMaterialBRDFInputs(PixelMaterial material, vec3 normal, vec3 view, vec3 light)
{
    return BuildBRDFInputs(normal, view, light, material.diffuseColor, material.f0, material.perceptualRoughness);
}

#endif