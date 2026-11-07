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
    ShaderCaps.hpp
    ShaderCaps.cpp
    Shader.hpp
    Shader.cpp
    Bindings.hpp
    Bindings.cpp
    Pipeline.hpp
    BindingPlan.cpp
    Lowering.cpp
    InputLayout.hpp
    InputLayout.cpp
    Pipeline.cpp
    # 即时列表按候选组合, 依赖保活, Draw 和传输拆分; 资源用途仍共用一份 Common tracker.
    CommandList.hpp
    CommandList.cpp
    CommandListState.cpp
    CommandListOwnership.cpp
    CommandListGraphics.cpp
    Graphics.cpp
    CommandListDraw.cpp
    CommandListUpload.cpp
    CommandListTransfer.cpp
    CommandListUnsupported.cpp
    Upload.hpp
    Upload.cpp
    Submission.hpp
    Submission.cpp
    # native 绑定/顶点格式与固定光栅 lowering 同属唯一 ContextState, 不形成另一套逻辑图形状态.
    ContextState.hpp
    ContextState.cpp
    ContextGraphics.cpp
)

# OpenGL.hpp 的公开签名使用 RHI 类型, 因而传递通用合同; glad 与 GLSL lowering 只供后端私有实现使用.
# spirv-cross-glsl 传递其 core 头/链接闭包, 不经 Engine/Shader 引入 glslang 或业务依赖.
target_link_libraries(PhysaraRHIOpenGL PUBLIC PhysaraRHI PRIVATE glad spirv-cross-glsl)
target_compile_features(PhysaraRHIOpenGL PUBLIC cxx_std_20)
set_target_properties(PhysaraRHIOpenGL PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRHIOpenGL PRIVATE /W4 /WX /utf-8 /permissive-)
