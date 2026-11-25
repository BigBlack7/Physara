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
physara_add_shader_program(StaticForward StaticForward ../Shaders/Passes/Forward/StaticForward.json ../Shaders PRODUCT)
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
physara_add_shader_program(ForwardSky ForwardSky ../Shaders/Passes/Forward/ForwardSky.json ../Shaders PRODUCT)
physara_add_shader_program(ForwardDisplay ForwardDisplay ../Shaders/Passes/Forward/ForwardDisplay.json ../Shaders PRODUCT)
foreach(program DisplayColorGrading DisplayFxaa DisplayScale)
    physara_add_shader_program(${program} DisplayPost ../Shaders/PostProcess/${program}.json ../Shaders PRODUCT)
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
)
target_link_libraries(PhysaraRenderer PUBLIC PhysaraCore PhysaraScene PhysaraRHI PhysaraRenderGraph
    PRIVATE PhysaraShader glm filament_display)
target_include_directories(PhysaraRenderer PRIVATE "${PROJECT_BINARY_DIR}/Generated/Shader/$<CONFIG>")
add_dependencies(PhysaraRenderer PhysaraShaderInterfaceStaticForward PhysaraShaderInterfaceForwardDisplay
    PhysaraShaderInterfaceForwardSky PhysaraShaderInterfaceDisplayPost)
target_compile_features(PhysaraRenderer PUBLIC cxx_std_20)
set_target_properties(PhysaraRenderer PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRenderer PRIVATE /W4 /WX /utf-8 /permissive-)

# Runtime 通过同一生成目标定位配置包, Renderer 只依赖生成的编译期接口.
foreach(program StaticForward StaticDepth ForwardDisplay StaticForwardPhysical StaticForwardIbl ForwardSky
        DisplayColorGrading DisplayFxaa DisplayScale StaticShadowDepth StaticCascadePreview
        StaticSpotShadowPreview StaticPointShadowPreview StaticForwardShadows StaticForwardPhysicalShadows
        StaticForwardIblShadows)
    set_property(TARGET PhysaraRenderer PROPERTY "PHYSARA_${program}_PACKAGE" "${${program}Package}")
    set_property(TARGET PhysaraRenderer PROPERTY "PHYSARA_${program}_TARGET" "${${program}PackageTarget}")
endforeach()
