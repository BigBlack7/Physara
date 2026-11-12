# 唯一产品入口装配诊断, 原生宿主, View 和 RHI Editor, 并按提交完成调度帧槽.
# Runtime 通过各模块的公开接口装配. GLFW 和 ImGui 头留在各自的私有实现.
add_executable(Physara Main.cpp Resources.hpp Resources.cpp)

# 从唯一包 target 取得配置产物, 生成相对实际 exe 目录的路径, 不固化本机绝对资源根.
get_target_property(editorUiPackage PhysaraEditor PHYSARA_UI_PACKAGE)
get_target_property(editorUiPackageTarget PhysaraEditor PHYSARA_UI_PACKAGE_TARGET)
add_dependencies(Physara ${editorUiPackageTarget})
# 每个构建配置生成自己的私有路径头. Resources.cpp 以 Win32 查询到的 exe 目录为基准拼接.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>/RuntimeResources.hpp"
    CONTENT "#pragma once\nnamespace Physara::Runtime\n{\n    inline constexpr auto EditorUiPackageRelative = \"$<PATH:RELATIVE_PATH,${editorUiPackage},$<TARGET_FILE_DIR:Physara>>\";\n}\n")
target_include_directories(Physara PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>")
target_link_libraries(Physara PRIVATE PhysaraPlatform PhysaraCore PhysaraEditor PhysaraRenderer)
target_compile_features(Physara PRIVATE cxx_std_20)
set_target_properties(Physara PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(Physara PRIVATE /W4 /WX /utf-8 /permissive-)
