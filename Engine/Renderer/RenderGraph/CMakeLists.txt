# 本目录的静态库负责 CPU 构图, 编译及按 Pass 解析资源的入口.
# 编译器确定存活顺序, 用途与存储规划分别生成后续执行所需的值.
# 公开声明使用 Core 的身份和帧内存, 以及通用 RHI 的描述和句柄, 因此传播这两个目标.
add_library(PhysaraRenderGraph STATIC
    GraphResources.hpp
    GraphRanges.hpp
    GraphContent.hpp
    GraphCompiler.hpp
    GraphCompilerInternal.hpp
    GraphCompiler.cpp
    GraphPlanStorage.cpp
    GraphPlanUses.cpp
    RenderGraph.hpp
    RenderGraph.cpp
    PassResources.cpp
)
target_link_libraries(PhysaraRenderGraph PUBLIC PhysaraCore PhysaraRHI)
target_compile_features(PhysaraRenderGraph PUBLIC cxx_std_20)
set_target_properties(PhysaraRenderGraph PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRenderGraph PRIVATE /W4 /WX /utf-8 /permissive-)
