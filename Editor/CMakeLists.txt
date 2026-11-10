# Editor 目标拥有 CPU UI 会话, 包含输入仲裁, 工作区及原生输入适配.
add_library(PhysaraEditor STATIC
    Session/EditorSession.hpp
    Session/EditorSession.cpp
    Session/ActionRouter.hpp
    Session/ActionRouter.cpp
    Panels/Workspace.hpp
    Panels/Workspace.cpp
    ImGui/GlfwPlatform.hpp
    ImGui/GlfwPlatform.cpp
)
target_include_directories(PhysaraEditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Session 的公开头使用 Platform 输入和请求值. ImGui 和 GLFW 用法留在私有实现.
# 适配直接读取归还后的指针位置, 因此私有链接 glfw.
target_link_libraries(PhysaraEditor PUBLIC PhysaraPlatform PRIVATE imgui glfw)
target_compile_definitions(PhysaraEditor PRIVATE GLFW_INCLUDE_NONE)
target_compile_features(PhysaraEditor PUBLIC cxx_std_20)
set_target_properties(PhysaraEditor PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraEditor PRIVATE /W4 /WX /utf-8 /permissive-)
