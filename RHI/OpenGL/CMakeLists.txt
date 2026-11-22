# 原生 GL 组件有实际消费者和 loader 依赖; 通用 PhysaraRHI 的独立闭包保持不变.
add_library(PhysaraRHIOpenGL STATIC
    ../OpenGL.hpp
    Backend.hpp
    Device.cpp
    Formats.cpp
    Unsupported.cpp
    Buffer.cpp
    # 公开 staging 的 footprint 和 CPU 借用独立于瞬态上传, 与实际传输共用物理包退休.
    Staging.hpp
    Staging.cpp
    CommandListStaging.cpp
    Texture.cpp
    Sampler.cpp
    Framebuffer.cpp
    # Queries 拥有对象/Device 查询入口, CommandListQueries 发出即时点; 共用提交完成/lease/删除 owner.
    Queries.hpp
    Queries.cpp
    CommandListQueries.cpp
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
    # Draw, Dispatch 和传输共用一个即时列表. CommandListState 接受公共用途, ShaderAccess 按实际访问发射同步.
    CommandList.hpp
    CommandList.cpp
    CommandListState.cpp
    ShaderAccess.cpp
    CommandListOwnership.cpp
    CommandListGraphics.cpp
    CommandListCompute.cpp
    Graphics.cpp
    CommandListDraw.cpp
    CommandListUpload.cpp
    CommandListTransfer.cpp
    CommandListUnsupported.cpp
    Upload.hpp
    Upload.cpp
    Submission.hpp
    Submission.cpp
    # 闭合列表的窗口末端复制与正常提交共用设备的保活和完成路径.
    WindowPresentation.cpp
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
