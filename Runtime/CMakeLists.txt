# 唯一产品入口装配诊断, CPU 资产, Scene, 原生宿主, View 和 Editor, 并按提交完成调度帧槽.
# Runtime 通过各模块的公开接口装配. GLFW 和 ImGui 头留在各自的私有实现.
# Frame 负责一次取得内的正常/安全图和取消, Main 负责输入与有界槽调度.
# FrameStatistics 随产品编译, 在录制及后续查询轮询处为 Editor 填充带票据的统计值.
add_executable(Physara Main.cpp Frame.hpp Frame.cpp FrameStatistics.hpp FrameStatistics.cpp Resources.hpp Resources.cpp)

# 从唯一包 target 取得配置产物, 生成相对实际 exe 目录的路径, 不固化本机绝对资源根.
get_target_property(editorUiPackage PhysaraEditor PHYSARA_UI_PACKAGE)
get_target_property(editorUiPackageTarget PhysaraEditor PHYSARA_UI_PACKAGE_TARGET)
add_dependencies(Physara ${editorUiPackageTarget})
# 将 Renderer 登记的全部包纳入产品构建. 帧前按实际受光或诊断角色加载对应包.
set(forwardResourcePaths "")
foreach(program StaticForward StaticCoreGBuffer StaticSurfaceAux StaticDepth ForwardDisplay StaticForwardPhysical StaticForwardIbl ForwardSky
        DisplayColorGrading DisplayFxaa DisplayScale StaticShadowDepth StaticCascadePreview
        StaticSpotShadowPreview StaticPointShadowPreview StaticForwardShadows StaticForwardPhysicalShadows
        StaticForwardIblShadows StaticForwardPlus StaticForwardPlusPhysical StaticForwardPlusIbl
        StaticForwardPlusShadows StaticForwardPlusPhysicalShadows StaticForwardPlusIblShadows)
    get_target_property(${program}Package PhysaraRenderer "PHYSARA_${program}_PACKAGE")
    get_target_property(packageTarget PhysaraRenderer "PHYSARA_${program}_TARGET")
    add_dependencies(Physara ${packageTarget})
    string(APPEND forwardResourcePaths
        "    inline constexpr auto ${program}PackageRelative = \"$<PATH:RELATIVE_PATH,${${program}Package},$<TARGET_FILE_DIR:Physara>>\";\n")
endforeach()
# 每个构建配置生成自己的私有路径头. Resources.cpp 以 Win32 查询到的 exe 目录为基准拼接.
# CPU 资产根直接使用该 exe 目录, 不把开发机的项目源目录写入产品.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>/RuntimeResources.hpp"
    CONTENT "#pragma once\nnamespace Physara::Runtime\n{\n    inline constexpr auto EditorUiPackageRelative = \"$<PATH:RELATIVE_PATH,${editorUiPackage},$<TARGET_FILE_DIR:Physara>>\";\n${forwardResourcePaths}}\n")
target_include_directories(Physara PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>")
# Runtime 装配开发会话的 Shader 源观察, 编译依赖保持在 Shader 模块中.
target_link_libraries(Physara PRIVATE PhysaraPlatform PhysaraCore PhysaraAsset PhysaraScene PhysaraEditor PhysaraRenderer
    PhysaraShader)
target_compile_features(Physara PRIVATE cxx_std_20)
set_target_properties(Physara PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(Physara PRIVATE /W4 /WX /utf-8 /permissive-)
