# 原生 GL 组件有实际消费者和 loader 依赖; 通用 PhysaraRHI 的独立闭包保持不变.
add_library(PhysaraRHIOpenGL STATIC
    ../OpenGL.hpp
    Backend.hpp
    Device.cpp
    Formats.cpp
    Unsupported.cpp
    Buffer.cpp
    Texture.cpp
    Sampler.cpp
    Framebuffer.cpp
)

target_link_libraries(PhysaraRHIOpenGL PUBLIC PhysaraRHI PRIVATE glad)
target_compile_features(PhysaraRHIOpenGL PUBLIC cxx_std_20)
set_target_properties(PhysaraRHIOpenGL PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRHIOpenGL PRIVATE /W4 /WX /utf-8 /permissive-)
