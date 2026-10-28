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
    # 即时列表/上传数据与提交退休分别拥有其状态, 共享唯一 Common tracker.
    CommandList.hpp
    CommandList.cpp
    CommandListState.cpp
    CommandListUpload.cpp
    CommandListUnsupported.cpp
    Upload.hpp
    Upload.cpp
    Submission.hpp
    Submission.cpp
    ContextState.hpp
    ContextState.cpp
)

target_link_libraries(PhysaraRHIOpenGL PUBLIC PhysaraRHI PRIVATE glad)
target_compile_features(PhysaraRHIOpenGL PUBLIC cxx_std_20)
set_target_properties(PhysaraRHIOpenGL PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRHIOpenGL PRIVATE /W4 /WX /utf-8 /permissive-)
