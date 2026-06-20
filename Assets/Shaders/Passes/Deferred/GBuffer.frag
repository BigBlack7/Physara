#version 460 core
#ifdef PHYSARA_BINDLESS_MATERIAL_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif
#extension GL_ARB_shading_language_include : require
#include "../../Includes/SurfaceMaterial.glsl"
#include "../../Includes/Packing.glsl"

layout(location = 0)in vec3 inWorldNormal;
layout(location = 1)in vec4 inWorldTangent;
layout(location = 2)in vec2 inTexCoord0;
layout(location = 3)in vec2 inTexCoord1;
layout(location = 4)flat in uint inMaterialIndex;
layout(location = 5)flat in uint inObjectFlags;

layout(location = 0)out vec4 outBaseColorAO;
layout(location = 1)out vec2 outNormal;
layout(location = 2)out vec4 outMaterial;
layout(location = 3)out vec4 outEmissive;

void main()
{
    MaterialInputs inputs = UnpackMaterialData(uMaterials[inMaterialIndex]);
    ApplySurfaceTextures(inputs, inMaterialIndex, inTexCoord0, inTexCoord1);
    PixelMaterial material = PrepareMaterial(inputs);
    if (ShouldDiscardMaterial(material))
    {
        discard;
    }

    vec3 geometricNormal = normalize(inWorldNormal);
    if (inputs.doubleSided && !gl_FrontFacing)
    {
        geometricNormal = -geometricNormal;
    }
    vec3 normal = ResolveSurfaceWorldNormal(
        inputs,
        inMaterialIndex,
        geometricNormal,
        inWorldTangent,
        inTexCoord0,
        inTexCoord1);
    outBaseColorAO = vec4(material.baseColor.rgb, material.ambientOcclusion);
    outNormal = PackNormalOctahedron(normal);
    outMaterial = vec4(
        material.perceptualRoughness,
        material.metallic,
        material.reflectance,
        (inObjectFlags & PHYSARA_OBJECT_RECEIVE_SHADOW) != 0u ? 1.0 : 0.0);
    outEmissive = vec4(material.emissive, 1.0);
}
