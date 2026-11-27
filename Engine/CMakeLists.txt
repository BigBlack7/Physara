# Engine 在此装配 Core, CPU Asset 和 Scene, 独立 RenderGraph, View 和 Shader 目标.
# Core 提供 CPU 基础机制. FrameArena 的复用时机由 Runtime 决定, 日志和 CNG 后端私有链接.
add_library(PhysaraCore STATIC
    Core/Identity.hpp
    Core/Jobs.hpp
    Core/ContentHash.hpp
    Core/ContentHash.cpp
    Core/ResourceIO.hpp
    Core/ResourceIO.cpp
    Core/FrameArena.hpp
    Core/FrameArena.cpp
    Core/Error.hpp
    Core/Error.cpp
    Core/Diagnostics.hpp
    Core/Diagnostics.cpp
)
target_include_directories(PhysaraCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PhysaraCore PRIVATE spdlog bcrypt)
target_compile_features(PhysaraCore PUBLIC cxx_std_20)
set_target_properties(PhysaraCore PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraCore PRIVATE /W4 /WX /utf-8 /permissive-)

# Asset 在 Core 的受控快照上执行图像, EXR 和模型导入. 解码依赖由资产目标私有链接.
add_subdirectory(Asset)
add_subdirectory(Scene)

# 独立图目标提供 CPU 声明和计划, 并通过公共 RHI 在借用的即时录制中执行.
add_subdirectory(Renderer/RenderGraph)

# PRODUCT 将目录角色传给打包驱动, 与热重载共用同一组阶段和结构值检查.
# 常规 Forward 颜色和主深度共享静态顶点入口. 受光角色按 DFG 和环境采样需求分别打包.
# 天空混入工作色. 显示分级, FXAA 和缩放各自生成包; 后两者共用 DisplayPost 的同一组活动绑定.
add_subdirectory(Shader)
physara_add_shader_interface(StaticForward Shader/Interface/StaticForward.json)
physara_add_shader_interface(ForwardDisplay Shader/Interface/ForwardDisplay.json)
physara_add_shader_interface(ForwardSky Shader/Interface/ForwardSky.json)
physara_add_shader_interface(DisplayPost Shader/Interface/DisplayPost.json)
physara_add_shader_interface(SurfacePreview Shader/Interface/SurfacePreview.json)
physara_add_shader_program(StaticForward StaticForward ../Shaders/Passes/Forward/StaticForward.json ../Shaders PRODUCT)
# 核心表面共用现有顶点和材质 ABI, 片元入口写四个固定类型附件.
physara_add_shader_program(StaticCoreGBuffer StaticForward ../Shaders/Passes/GBuffer/StaticCoreGBuffer.json ../Shaders PRODUCT)
# 辅助表面共用静态顶点接口, 仅向 G1 和 G3 写颜色附件.
physara_add_shader_program(StaticSurfaceAux StaticForward ../Shaders/Passes/GBuffer/StaticSurfaceAux.json ../Shaders PRODUCT)
physara_add_shader_program(StaticDepth StaticForward ../Shaders/Passes/Forward/StaticDepth.json ../Shaders PRODUCT)
# 阴影深度使用专用投影顶点和共享 MASK 片元. 各诊断包复用数组查询, 以各自的入口呈现级联或灯面.
physara_add_shader_program(StaticShadowDepth StaticForward ../Shaders/Passes/Shadow/StaticShadowDepth.json ../Shaders PRODUCT)
physara_add_shader_program(StaticCascadePreview StaticForward ../Shaders/Passes/Shadow/StaticCascadePreview.json ../Shaders PRODUCT)
physara_add_shader_program(StaticSpotShadowPreview StaticForward ../Shaders/Passes/Shadow/StaticSpotShadowPreview.json ../Shaders PRODUCT)
physara_add_shader_program(StaticPointShadowPreview StaticForward ../Shaders/Passes/Shadow/StaticPointShadowPreview.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPhysical StaticForward ../Shaders/Passes/Forward/StaticForwardPhysical.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardIbl StaticForward ../Shaders/Passes/Forward/StaticForwardIbl.json ../Shaders PRODUCT)
# 受光 Pass 读取数组时使用独立活动绑定, Hard 和 PCF 共用同一程序.
physara_add_shader_program(StaticForwardShadows StaticForward ../Shaders/Passes/Forward/StaticForwardShadows.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPhysicalShadows StaticForward ../Shaders/Passes/Forward/StaticForwardPhysicalShadows.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardIblShadows StaticForward ../Shaders/Passes/Forward/StaticForwardIblShadows.json ../Shaders PRODUCT)
# Froxel 只扩展不透明颜色角色, 资源合同沿用 StaticForward 结构定义.
physara_add_shader_program(StaticForwardPlus StaticForward ../Shaders/Passes/Forward/StaticForwardPlus.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPlusPhysical StaticForward ../Shaders/Passes/Forward/StaticForwardPlusPhysical.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPlusIbl StaticForward ../Shaders/Passes/Forward/StaticForwardPlusIbl.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPlusShadows StaticForward ../Shaders/Passes/Forward/StaticForwardPlusShadows.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPlusPhysicalShadows StaticForward ../Shaders/Passes/Forward/StaticForwardPlusPhysicalShadows.json ../Shaders PRODUCT)
physara_add_shader_program(StaticForwardPlusIblShadows StaticForward ../Shaders/Passes/Forward/StaticForwardPlusIblShadows.json ../Shaders PRODUCT)
# IOR/specular 只为受光颜色生成包. 顺序对应 ProductShader 的全灯与 Froxel 角色.
set(iorSpecularPrograms
    StaticForwardIorSpecular StaticForwardIorSpecularPhysical StaticForwardIorSpecularIbl
    StaticForwardIorSpecularShadows StaticForwardIorSpecularPhysicalShadows StaticForwardIorSpecularIblShadows
    StaticForwardPlusIorSpecular StaticForwardPlusIorSpecularPhysical StaticForwardPlusIorSpecularIbl
    StaticForwardPlusIorSpecularShadows StaticForwardPlusIorSpecularPhysicalShadows StaticForwardPlusIorSpecularIblShadows
)
foreach(program IN LISTS iorSpecularPrograms)
    physara_add_shader_program(${program} StaticForward ../Shaders/Passes/Forward/${program}.json ../Shaders PRODUCT)
endforeach()
# 核心屏幕光照使用相同的灯光 ABI. 每个 role 只建立一个 RGBA16F 全屏 Pipeline.
set(deferredPrograms DeferredLighting DeferredLightingPhysical DeferredLightingIbl DeferredLightingShadows
    DeferredLightingPhysicalShadows DeferredLightingIblShadows)
foreach(program IN LISTS deferredPrograms)
    physara_add_shader_program(${program} StaticForward ../Shaders/Passes/Deferred/${program}.json ../Shaders PRODUCT)
endforeach()
physara_add_shader_program(ForwardSky ForwardSky ../Shaders/Passes/Forward/ForwardSky.json ../Shaders PRODUCT)
physara_add_shader_program(ForwardDisplay ForwardDisplay ../Shaders/Passes/Forward/ForwardDisplay.json ../Shaders PRODUCT)
foreach(program DisplayColorGrading DisplayFxaa DisplayScale)
    physara_add_shader_program(${program} DisplayPost ../Shaders/PostProcess/${program}.json ../Shaders PRODUCT)
endforeach()
# 表面诊断按深度或混合颜色类型准备原生配方, 共用一张普通 UI 颜色目标.
set(surfacePreviewPrograms SurfaceDepthPreview SurfaceGBufferPreview)
foreach(program IN LISTS surfacePreviewPrograms)
    physara_add_shader_program(${program} SurfacePreview ../Shaders/Passes/SurfacePreview/${program}.json ../Shaders PRODUCT)
endforeach()

# Renderer 管理只读 Scene/View 工作集、生效颜色代及静态 Forward 准备与图声明.
# 公开头使用 Scene 的位姿/身份和图值, 因此传播 Scene 与 RenderGraph 依赖.
# 主视锥和阴影体在 Renderer 内共享 GLM 运算. 阴影规划接收设备上限的值, 无需链接具体后端.
# View 准备头以前置声明借出 Froxel 载荷. Renderer 的 Lighting 头在模块内包含当前配置生成的数据布局.
# Editor 与 Runtime 经此依赖消费公共图接口; Renderer 本身不反向依赖窗口或 UI.
add_library(PhysaraRenderer STATIC
    Renderer/View.hpp Renderer/View.cpp
    Renderer/FrameGraph.hpp Renderer/FrameGraph.cpp
    Renderer/Prepare/SceneFrame.hpp Renderer/Prepare/SceneFrame.cpp
    Renderer/Prepare/CameraSnapshot.hpp
    Renderer/Prepare/ViewFrustum.hpp
    Renderer/Prepare/ViewPreparation.hpp Renderer/Prepare/ViewPreparation.cpp
    Renderer/Lighting/PunctualLights.hpp Renderer/Lighting/PunctualLights.cpp
    Renderer/Lighting/Froxel.hpp Renderer/Lighting/Froxel.cpp
    Renderer/Lighting/FroxelGpu.hpp Renderer/Lighting/FroxelGpu.cpp
    Renderer/Lighting/LightVolume.hpp
    Renderer/Lighting/ShadowPlan.hpp Renderer/Lighting/ShadowPlan.cpp
    Renderer/Lighting/ShadowVolumes.hpp Renderer/Lighting/ShadowVolumes.cpp
    Renderer/Lighting/EnvironmentBake.hpp Renderer/Lighting/EnvironmentBake.cpp
    Renderer/Lighting/EnvironmentCache.hpp Renderer/Lighting/EnvironmentCache.cpp
    Renderer/Lighting/EnvironmentGpu.cpp
    Renderer/Materials/StaticGpu.hpp Renderer/Materials/StaticGpu.cpp Renderer/Materials/StaticTextures.cpp
    Renderer/Draw/StaticDraw.cpp
    Renderer/PostProcess/ColorLut.hpp Renderer/PostProcess/ColorLut.cpp
    Renderer/PostProcess/Display.hpp Renderer/PostProcess/Display.cpp
    Renderer/Paths/Forward.hpp Renderer/Paths/Forward.cpp
    Renderer/Paths/ForwardShaderKeys.hpp Renderer/Paths/ForwardShaderPreparation.cpp
    Renderer/Paths/SurfacePreview.cpp
)
target_link_libraries(PhysaraRenderer PUBLIC PhysaraCore PhysaraScene PhysaraRHI PhysaraRenderGraph
    PRIVATE PhysaraShader glm filament_display)
target_include_directories(PhysaraRenderer PRIVATE "${PROJECT_BINARY_DIR}/Generated/Shader/$<CONFIG>")
add_dependencies(PhysaraRenderer PhysaraShaderInterfaceStaticForward PhysaraShaderInterfaceForwardDisplay
    PhysaraShaderInterfaceForwardSky PhysaraShaderInterfaceDisplayPost PhysaraShaderInterfaceSurfacePreview)
target_compile_features(PhysaraRenderer PUBLIC cxx_std_20)
set_target_properties(PhysaraRenderer PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRenderer PRIVATE /W4 /WX /utf-8 /permissive-)

# Runtime 通过同一生成目标定位配置包, Renderer 只依赖生成的编译期接口.
set_property(TARGET PhysaraRenderer PROPERTY PHYSARA_IOR_SPECULAR_PROGRAMS "${iorSpecularPrograms}")
foreach(program StaticForward StaticCoreGBuffer StaticSurfaceAux StaticDepth ForwardDisplay StaticForwardPhysical StaticForwardIbl ForwardSky
        DisplayColorGrading DisplayFxaa DisplayScale StaticShadowDepth StaticCascadePreview
        StaticSpotShadowPreview StaticPointShadowPreview StaticForwardShadows StaticForwardPhysicalShadows
        StaticForwardIblShadows StaticForwardPlus StaticForwardPlusPhysical StaticForwardPlusIbl
        StaticForwardPlusShadows StaticForwardPlusPhysicalShadows StaticForwardPlusIblShadows ${deferredPrograms}
        ${surfacePreviewPrograms} ${iorSpecularPrograms})
    set_property(TARGET PhysaraRenderer PROPERTY "PHYSARA_${program}_PACKAGE" "${${program}Package}")
    set_property(TARGET PhysaraRenderer PROPERTY "PHYSARA_${program}_TARGET" "${${program}PackageTarget}")
endforeach()
