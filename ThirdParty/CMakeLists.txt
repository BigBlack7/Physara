# 头文件库只提供 include 路径, 不生成 .lib.
# SYSTEM 将上游头文件诊断与第一方代码的警告要求分开.
add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE glm)

add_library(entt INTERFACE)
target_include_directories(entt SYSTEM INTERFACE entt/single_include)

add_library(spdlog INTERFACE)
target_include_directories(spdlog SYSTEM INTERFACE spdlog/include)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE json/single_include)

# stb 是单头实现库; 后续在实际图像解码的一个 cpp 中定义实现宏.
# 此处不额外创建包装 cpp, 也不向其他编译单元传播实现宏.
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE stb)

# GLFW: 窗口, 输入和 OpenGL context.保留 Windows 后端及其公共/null 实现.
add_library(glfw STATIC
    glfw/src/context.c
    glfw/src/init.c
    glfw/src/input.c
    glfw/src/monitor.c
    glfw/src/platform.c
    glfw/src/vulkan.c
    glfw/src/window.c
    glfw/src/egl_context.c
    glfw/src/osmesa_context.c
    glfw/src/null_init.c
    glfw/src/null_monitor.c
    glfw/src/null_window.c
    glfw/src/null_joystick.c
    glfw/src/win32_module.c
    glfw/src/win32_time.c
    glfw/src/win32_thread.c
    glfw/src/win32_init.c
    glfw/src/win32_joystick.c
    glfw/src/win32_monitor.c
    glfw/src/win32_window.c
    glfw/src/wgl_context.c
)
target_include_directories(glfw SYSTEM PUBLIC glfw/include)
target_compile_definitions(glfw PRIVATE _GLFW_WIN32 UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(glfw PRIVATE gdi32)

# GLAD: 已生成并固定的 OpenGL 4.6 Core 函数加载器, 构建时不运行生成器.
add_library(glad STATIC glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC glad/include)

# ImGui 只编译核心和 GLFW 输入适配; 绘制适配由后续 Editor/RHI 实现.
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui SYSTEM PUBLIC imgui imgui/backends)
target_compile_definitions(imgui PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(imgui PRIVATE glfw)

# ImGuizmo 的公开头使用 ImGui 类型, 因此将 ImGui 依赖传递给消费者.
add_library(imguizmo STATIC imguizmo/src/ImGuizmo.cpp)
target_include_directories(imguizmo SYSTEM PUBLIC imguizmo/src)
target_link_libraries(imguizmo PUBLIC imgui)

# TinyGLTF 3 使用上游 C11 实现; JSON 场景文件由 nlohmann_json 单独处理.
add_library(tinygltf STATIC tinygltf/tiny_gltf_v3.c)
target_include_directories(tinygltf SYSTEM PUBLIC tinygltf)
set_target_properties(tinygltf PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# TinyEXR 使用上游实现文件, 配套 miniz 仅供其内部压缩/解压使用.
add_library(tinyexr STATIC tinyexr/tinyexr.cc tinyexr/deps/miniz/miniz.c)
target_include_directories(tinyexr SYSTEM PUBLIC tinyexr PRIVATE tinyexr/deps/miniz)

foreach(target IN ITEMS imgui imguizmo tinyexr)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endforeach()

# 编译编码仅作用于这些第三方 target, 不设置全局编译选项.
foreach(target IN ITEMS glfw glad imgui imguizmo tinygltf tinyexr)
    target_compile_options(${target} PRIVATE /utf-8)
endforeach()

# MikkTSpace 在资产导入实际需要生成切线时接入, 目前仅保留源码.
# NVRHI 仅供 RHI 开发参考, 不添加 target, include 路径或链接依赖.
