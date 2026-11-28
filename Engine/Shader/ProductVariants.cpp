#include "ProductVariants.hpp"

/// @file
/// @brief 把产品程序名和结构字段解析为唯一的目录项, 为离线打包和重载提供相同的阶段入口.

#include <algorithm>
#include <charconv>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    // 角色值也是包数组下标. Direct, DFG 和 IBL 按候选来源及是否读取阴影数组选择片元入口.
    // 三种诊断共用数组查询, 以入口决定配色. Hard/PCF 和级联数量均随本帧数据传入.
    static constexpr std::array<ProductProgram, ProductShaderCount> Programs{{
        {{ProductShader::StaticForward}, "StaticForward", "StaticForward",
         "Passes/Forward/StaticForward.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticDepth}, "StaticDepth", "StaticForward",
         "Passes/Forward/StaticDepth.json", "Passes/Forward/Static.vert", "Passes/Forward/Depth.frag"},
        {{ProductShader::ForwardDisplay}, "ForwardDisplay", "ForwardDisplay",
         "Passes/Forward/ForwardDisplay.json", "Passes/Forward/Display.vert", "Passes/Forward/Display.frag"},
        {{ProductShader::StaticForwardPhysical, 1}, "StaticForwardPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPhysical.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIbl, 2}, "StaticForwardIbl", "StaticForward",
         "Passes/Forward/StaticForwardIbl.json", "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::ForwardSky}, "ForwardSky", "ForwardSky",
         "Passes/Forward/ForwardSky.json", "Passes/Forward/Sky.vert", "Passes/Forward/Sky.frag"},
        {{ProductShader::DisplayColorGrading}, "DisplayColorGrading", "DisplayPost",
         "PostProcess/DisplayColorGrading.json", "Passes/Forward/Display.vert", "PostProcess/DisplayColorGrading.frag"},
        {{ProductShader::DisplayFxaa}, "DisplayFxaa", "DisplayPost",
         "PostProcess/DisplayFxaa.json", "Passes/Forward/Display.vert", "PostProcess/DisplayFxaa.frag"},
        {{ProductShader::DisplayScale}, "DisplayScale", "DisplayPost",
         "PostProcess/DisplayScale.json", "Passes/Forward/Display.vert", "PostProcess/DisplayScale.frag"},
        {{ProductShader::StaticShadowDepth}, "StaticShadowDepth", "StaticForward",
         "Passes/Shadow/StaticShadowDepth.json", "Passes/Shadow/Depth.vert", "Passes/Forward/Depth.frag"},
        {{ProductShader::StaticCascadePreview}, "StaticCascadePreview", "StaticForward",
         "Passes/Shadow/StaticCascadePreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/CascadePreview.frag"},
        {{ProductShader::StaticSpotShadowPreview}, "StaticSpotShadowPreview", "StaticForward",
         "Passes/Shadow/StaticSpotShadowPreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/SpotPreview.frag"},
        {{ProductShader::StaticPointShadowPreview}, "StaticPointShadowPreview", "StaticForward",
         "Passes/Shadow/StaticPointShadowPreview.json", "Passes/Forward/Static.vert", "Passes/Shadow/PointPreview.frag"},
        {{ProductShader::StaticForwardShadows}, "StaticForwardShadows", "StaticForward",
         "Passes/Forward/StaticForwardShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPhysicalShadows, 1}, "StaticForwardPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPhysicalShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIblShadows, 2}, "StaticForwardIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardIblShadows.json", "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlus}, "StaticForwardPlus", "StaticForward",
         "Passes/Forward/StaticForwardPlus.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusPhysical, 1}, "StaticForwardPlusPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusPhysical.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIbl, 2}, "StaticForwardPlusIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusIbl.json", "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusShadows}, "StaticForwardPlusShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusPhysicalShadows, 1}, "StaticForwardPlusPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIblShadows, 2}, "StaticForwardPlusIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticCoreGBuffer}, "StaticCoreGBuffer", "StaticForward",
         "Passes/GBuffer/StaticCoreGBuffer.json", "Passes/Forward/Static.vert", "Passes/GBuffer/Core.frag"},
        {{ProductShader::StaticSurfaceAux}, "StaticSurfaceAux", "StaticForward",
         "Passes/GBuffer/StaticSurfaceAux.json", "Passes/Forward/Static.vert", "Passes/GBuffer/SurfaceAux.frag"},
        {{ProductShader::DeferredLighting}, "DeferredLighting", "StaticForward",
         "Passes/Deferred/DeferredLighting.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingPhysical, 1}, "DeferredLightingPhysical", "StaticForward",
         "Passes/Deferred/DeferredLightingPhysical.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingIbl, 2}, "DeferredLightingIbl", "StaticForward",
         "Passes/Deferred/DeferredLightingIbl.json", "Passes/Forward/Display.vert", "Passes/Deferred/Lighting.frag"},
        {{ProductShader::DeferredLightingShadows}, "DeferredLightingShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::DeferredLightingPhysicalShadows, 1}, "DeferredLightingPhysicalShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingPhysicalShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::DeferredLightingIblShadows, 2}, "DeferredLightingIblShadows", "StaticForward",
         "Passes/Deferred/DeferredLightingIblShadows.json", "Passes/Forward/Display.vert",
         "Passes/Deferred/LightingShadows.frag"},
        {{ProductShader::SurfaceDepthPreview}, "SurfaceDepthPreview", "SurfacePreview",
         "Passes/SurfacePreview/SurfaceDepthPreview.json", "Passes/Forward/Display.vert",
         "Passes/SurfacePreview/Depth.frag"},
        {{ProductShader::SurfaceGBufferPreview}, "SurfaceGBufferPreview", "SurfacePreview",
         "Passes/SurfacePreview/SurfaceGBufferPreview.json", "Passes/Forward/Display.vert",
         "Passes/SurfacePreview/GBuffer.frag"},
        {{ProductShader::StaticForwardIorSpecular, 0, 1}, "StaticForwardIorSpecular", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecular.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularPhysical, 1, 1}, "StaticForwardIorSpecularPhysical", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularIbl, 2, 1}, "StaticForwardIorSpecularIbl", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardIorSpecularShadows, 0, 1}, "StaticForwardIorSpecularShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIorSpecularPhysicalShadows, 1, 1}, "StaticForwardIorSpecularPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardIorSpecularIblShadows, 2, 1}, "StaticForwardIorSpecularIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardIorSpecularIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecular, 0, 1}, "StaticForwardPlusIorSpecular", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecular.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularPhysical, 1, 1}, "StaticForwardPlusIorSpecularPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularIbl, 2, 1}, "StaticForwardPlusIorSpecularIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularShadows, 0, 1}, "StaticForwardPlusIorSpecularShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularPhysicalShadows, 1, 1}, "StaticForwardPlusIorSpecularPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusIorSpecularIblShadows, 2, 1}, "StaticForwardPlusIorSpecularIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusIorSpecularIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoat, 0, 0, 1}, "StaticForwardClearcoat", "StaticForward",
         "Passes/Forward/StaticForwardClearcoat.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatPhysical, 1, 0, 1}, "StaticForwardClearcoatPhysical", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatIbl, 2, 0, 1}, "StaticForwardClearcoatIbl", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardClearcoatShadows, 0, 0, 1}, "StaticForwardClearcoatShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoatPhysicalShadows, 1, 0, 1}, "StaticForwardClearcoatPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardClearcoatIblShadows, 2, 0, 1}, "StaticForwardClearcoatIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardClearcoatIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoat, 0, 0, 1}, "StaticForwardPlusClearcoat", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoat.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatPhysical, 1, 0, 1}, "StaticForwardPlusClearcoatPhysical", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatPhysical.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatIbl, 2, 0, 1}, "StaticForwardPlusClearcoatIbl", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatIbl.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatShadows, 0, 0, 1}, "StaticForwardPlusClearcoatShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatPhysicalShadows, 1, 0, 1}, "StaticForwardPlusClearcoatPhysicalShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatPhysicalShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusClearcoatIblShadows, 2, 0, 1}, "StaticForwardPlusClearcoatIblShadows", "StaticForward",
         "Passes/Forward/StaticForwardPlusClearcoatIblShadows.json", "Passes/Forward/Static.vert",
         "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardSheen, 0, 0, 0, 1},
         "StaticForwardSheen", "StaticForward", "Passes/Forward/StaticForwardSheen.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenPhysical, 1, 0, 0, 1},
         "StaticForwardSheenPhysical", "StaticForward", "Passes/Forward/StaticForwardSheenPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenIbl, 2, 0, 0, 1},
         "StaticForwardSheenIbl", "StaticForward", "Passes/Forward/StaticForwardSheenIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenShadows, 0, 0, 0, 1},
         "StaticForwardSheenShadows", "StaticForward", "Passes/Forward/StaticForwardSheenShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardSheenPhysicalShadows, 1, 0, 0, 1},
         "StaticForwardSheenPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardSheenPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardSheenIblShadows, 2, 0, 0, 1},
         "StaticForwardSheenIblShadows", "StaticForward", "Passes/Forward/StaticForwardSheenIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheen, 0, 0, 0, 1},
         "StaticForwardPlusSheen", "StaticForward", "Passes/Forward/StaticForwardPlusSheen.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenPhysical, 1, 0, 0, 1},
         "StaticForwardPlusSheenPhysical", "StaticForward", "Passes/Forward/StaticForwardPlusSheenPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenIbl, 2, 0, 0, 1},
         "StaticForwardPlusSheenIbl", "StaticForward", "Passes/Forward/StaticForwardPlusSheenIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenShadows, 0, 0, 0, 1},
         "StaticForwardPlusSheenShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheenPhysicalShadows, 1, 0, 0, 1},
         "StaticForwardPlusSheenPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheenIblShadows, 2, 0, 0, 1},
         "StaticForwardPlusSheenIblShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardSheenClearcoat, 0, 0, 1, 1},
         "StaticForwardSheenClearcoat", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoat.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenClearcoatPhysical, 1, 0, 1, 1},
         "StaticForwardSheenClearcoatPhysical", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoatPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenClearcoatIbl, 2, 0, 1, 1},
         "StaticForwardSheenClearcoatIbl", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoatIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardSheenClearcoatShadows, 0, 0, 1, 1},
         "StaticForwardSheenClearcoatShadows", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoatShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardSheenClearcoatPhysicalShadows, 1, 0, 1, 1},
         "StaticForwardSheenClearcoatPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoatPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardSheenClearcoatIblShadows, 2, 0, 1, 1},
         "StaticForwardSheenClearcoatIblShadows", "StaticForward", "Passes/Forward/StaticForwardSheenClearcoatIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoat, 0, 0, 1, 1},
         "StaticForwardPlusSheenClearcoat", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoat.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoatPhysical, 1, 0, 1, 1},
         "StaticForwardPlusSheenClearcoatPhysical", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoatPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoatIbl, 2, 0, 1, 1},
         "StaticForwardPlusSheenClearcoatIbl", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoatIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoatShadows, 0, 0, 1, 1},
         "StaticForwardPlusSheenClearcoatShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoatShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoatPhysicalShadows, 1, 0, 1, 1},
         "StaticForwardPlusSheenClearcoatPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoatPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusSheenClearcoatIblShadows, 2, 0, 1, 1},
         "StaticForwardPlusSheenClearcoatIblShadows", "StaticForward", "Passes/Forward/StaticForwardPlusSheenClearcoatIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardAnisotropy, 0, 0, 0, 0, 1},
         "StaticForwardAnisotropy", "StaticForward", "Passes/Forward/StaticForwardAnisotropy.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardAnisotropyPhysical, 1, 0, 0, 0, 1},
         "StaticForwardAnisotropyPhysical", "StaticForward", "Passes/Forward/StaticForwardAnisotropyPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardAnisotropyIbl, 2, 0, 0, 0, 1},
         "StaticForwardAnisotropyIbl", "StaticForward", "Passes/Forward/StaticForwardAnisotropyIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/Color.frag"},
        {{ProductShader::StaticForwardAnisotropyShadows, 0, 0, 0, 0, 1},
         "StaticForwardAnisotropyShadows", "StaticForward", "Passes/Forward/StaticForwardAnisotropyShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardAnisotropyPhysicalShadows, 1, 0, 0, 0, 1},
         "StaticForwardAnisotropyPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardAnisotropyPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardAnisotropyIblShadows, 2, 0, 0, 0, 1},
         "StaticForwardAnisotropyIblShadows", "StaticForward", "Passes/Forward/StaticForwardAnisotropyIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/ShadowColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropy, 0, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropy", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropy.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropyPhysical, 1, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropyPhysical", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropyPhysical.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropyIbl, 2, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropyIbl", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropyIbl.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropyShadows, 0, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropyShadows", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropyShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropyPhysicalShadows, 1, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropyPhysicalShadows", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropyPhysicalShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
        {{ProductShader::StaticForwardPlusAnisotropyIblShadows, 2, 0, 0, 0, 1},
         "StaticForwardPlusAnisotropyIblShadows", "StaticForward", "Passes/Forward/StaticForwardPlusAnisotropyIblShadows.json",
         "Passes/Forward/Static.vert", "Passes/Forward/FroxelShadowColor.frag"},
    }};

    // 名称查找和按枚举索引须落在同一项, 重名或错位在构建目录时就会报错.
    static_assert([]
    {
        for (std::size_t index = 0; index < Programs.size(); ++index)
        {
            if (static_cast<std::size_t>(Programs[index].key.role) != index ||
                std::ranges::count(Programs, Programs[index].name, &ProductProgram::name) != 1)
            {
                return false;
            }
        }
        return true;
    }());

    std::span<const ProductProgram, ProductShaderCount> ProductPrograms() noexcept
    {
        return Programs;
    }

    const ProductProgram *FindProductProgram(ProductShader role) noexcept
    {
        const auto index = static_cast<std::size_t>(role);
        return index < Programs.size() ? &Programs[index] : nullptr;
    }

    const ProductProgram *FindProductProgram(std::string_view name) noexcept
    {
        const auto found = std::ranges::find(Programs, name, &ProductProgram::name);
        return found == Programs.end() ? nullptr : &*found;
    }

    // 结构值为 uint32_t, 固定缓冲可容纳全部十进制位. 错误路径无需分配.
    static void AppendValue(Core::BoundedText &description, std::uint32_t value) noexcept
    {
        std::array<char, 10> digits;
        const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
        description.Append({digits.data(), static_cast<std::size_t>(result.ptr - digits.data())});
    }

    Result<VariantKey> ResolveVariant(std::string_view name, std::span<const VariantField> fields) noexcept
    {
        const auto *program = FindProductProgram(name);
        if (!program)
        {
            return MakeFailure(ErrorCode::UnsupportedProfile, "Unknown product Shader role.");
        }
        VariantKey key{program->key.role};
        constexpr std::array members{
            std::pair{PhysicalLightingField, &VariantKey::physicalLighting},
            std::pair{IorSpecularField, &VariantKey::iorSpecular},
            std::pair{ClearcoatField, &VariantKey::clearcoat},
            std::pair{SheenField, &VariantKey::sheen}, std::pair{AnisotropyField, &VariantKey::anisotropy}};
        for (const auto &field : fields)
        {
            const auto found = std::ranges::find_if(members, [&](const auto &entry) { return entry.first == field.name; });
            const auto member = found == members.end() ? nullptr : found->second;
            if (!member || key.*member != 0 || field.value != program->key.*member || field.value == 0)
            {
                Core::BoundedText reason("/program/defines: ");
                reason.Append(field.name);
                reason.Append("=");
                AppendValue(reason, field.value);
                if (member && key.*member != 0)
                {
                    reason.Append(" duplicates this field in the product variant.");
                }
                else
                {
                    reason.Append(" is not allowed for ");
                    reason.Append(program->name);
                    if (member && program->key.*member != 0)
                    {
                        reason.Append(". The approved combination requires ");
                        reason.Append(found->first);
                        reason.Append("=");
                        AppendValue(reason, program->key.*member);
                        reason.Append(".");
                    }
                    else
                    {
                        reason.Append(". This field is outside the role's declared variant.");
                    }
                }
                return MakeFailure(ErrorCode::UnsupportedProfile, reason.View());
            }
            key.*member = field.value;
        }
        for (const auto &[fieldName, member] : members)
        {
            if (key.*member != program->key.*member)
            {
                Core::BoundedText reason("/program/defines: missing required ");
                reason.Append(fieldName);
                reason.Append(" field.");
                return MakeFailure(ErrorCode::UnsupportedProfile, reason.View());
            }
        }
        return key;
    }
}
