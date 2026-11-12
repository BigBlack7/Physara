# Editor 声明 UI 的共享接口和正式程序, 按配置生成绑定头和 Shader 包.
# 包 target 是唯一写入者, 沿生成接口和源依赖决定何时重新编译包.
physara_add_shader_interface(EditorUi "../Engine/Shader/Interface/EditorUi.json")
physara_add_shader_program(EditorUi EditorUi "../Shaders/UI/EditorUi.json" "../Shaders")

# Editor 拥有 UI 会话, 编辑历史, 工作区, 原生输入适配及公共 RHI 绘制入口.
# Commands 随会话编译, 通过 Renderer 的公开 View 入口应用及重放比例.
add_library(PhysaraEditor STATIC
    Commands/EditHistory.hpp
    Commands/EditHistory.cpp
    Session/EditorSession.hpp
    Session/EditorSession.cpp
    Session/UiFrame.hpp
    Session/ActionRouter.hpp
    Session/ActionRouter.cpp
    Panels/Workspace.hpp
    Panels/Workspace.cpp
    Viewport/Viewport.hpp
    Viewport/Viewport.cpp
    ImGui/GlfwPlatform.hpp
    ImGui/GlfwPlatform.cpp
    ImGui/UiRenderer.hpp
    ImGui/UiRenderer.cpp
    ImGui/UiTextures.hpp
    ImGui/UiTextures.cpp
)
# 编译 renderer 前先生成绑定头和匹配的包. 只有 Editor 实现包含这些生成头.
add_dependencies(PhysaraEditor PhysaraShaderInterfaceEditorUi ${EditorUiPackageTarget})
# Runtime 通过目标属性共享同一个生成前置和按配置区分的包路径.
set_property(TARGET PhysaraEditor PROPERTY PHYSARA_UI_PACKAGE "${EditorUiPackage}")
set_property(TARGET PhysaraEditor PROPERTY PHYSARA_UI_PACKAGE_TARGET "${EditorUiPackageTarget}")
target_include_directories(PhysaraEditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${PROJECT_BINARY_DIR}/Generated/Shader/$<CONFIG>")
# Session 公开 Platform 输入, RHI 绘制值和 Renderer 的 View 请求. Shader 加载及 ImGui/GLFW 留在私有实现.
# 适配直接读取归还后的指针位置, 因此私有链接 glfw.
target_link_libraries(PhysaraEditor PUBLIC PhysaraPlatform PhysaraRHI PhysaraRenderer PRIVATE PhysaraShader imgui glfw)
target_compile_definitions(PhysaraEditor PRIVATE GLFW_INCLUDE_NONE)
target_compile_features(PhysaraEditor PUBLIC cxx_std_20)
set_target_properties(PhysaraEditor PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraEditor PRIVATE /W4 /WX /utf-8 /permissive-)
