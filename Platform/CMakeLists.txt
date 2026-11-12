# Platform 拥有 GLFW 窗口, context, RHI 装配和窗口呈现的寿命.
# WindowSettings 在此实现 Win32 标题和 WGL 交换区间的原生确认.
add_library(PhysaraPlatform STATIC
    DeviceHost.hpp
    DeviceHost.cpp
    DeviceHostInternal.hpp
    GlfwAccess.hpp
    GlfwWindow.cpp
    OpenGLDeviceHost.cpp
    WindowSettings.cpp
    Presentation.hpp
    Presentation.cpp
    RhiDeviceHost.cpp
    Input.hpp
    InputState.hpp
    InputState.cpp
    GlfwInput.cpp
)
target_include_directories(PhysaraPlatform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# 公开宿主值包含 Core 和 RHI 句柄. 私有宿主实现消费 GL 后端, GLFW 和 Win32 交换入口.
target_link_libraries(PhysaraPlatform PUBLIC PhysaraCore PhysaraRHI PRIVATE PhysaraRHIOpenGL glfw glad opengl32 gdi32)
target_compile_definitions(PhysaraPlatform PRIVATE GLFW_INCLUDE_NONE)
target_compile_features(PhysaraPlatform PUBLIC cxx_std_20)
set_target_properties(PhysaraPlatform PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraPlatform PRIVATE /W4 /WX /utf-8 /permissive-)
