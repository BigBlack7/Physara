# Platform 独占 GLFW/window/context 和 RHI 装配寿命; 公开头仅前置声明 Device.
add_library(PhysaraPlatform STATIC
    DeviceHost.hpp
    DeviceHost.cpp
    DeviceHostInternal.hpp
    GlfwAccess.hpp
    GlfwWindow.cpp
    OpenGLDeviceHost.cpp
    RhiDeviceHost.cpp
    Input.hpp
    InputState.hpp
    InputState.cpp
    GlfwInput.cpp
)
target_include_directories(PhysaraPlatform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PhysaraPlatform PUBLIC PhysaraCore PRIVATE PhysaraRHIOpenGL glfw glad)
target_compile_definitions(PhysaraPlatform PRIVATE GLFW_INCLUDE_NONE)
target_compile_features(PhysaraPlatform PUBLIC cxx_std_20)
set_target_properties(PhysaraPlatform PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraPlatform PRIVATE /W4 /WX /utf-8 /permissive-)
