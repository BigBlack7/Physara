# Engine 在此装配 Core, CPU Asset, 独立 RenderGraph, View 和 Shader 目标.
# Core 提供 CPU 基础机制. FrameArena 的复用时机由 Runtime 决定, 日志和 CNG 后端私有链接.
add_library(PhysaraCore STATIC
    Core/Identity.hpp
    Core/Jobs.hpp
    Core/ContentHash.hpp
    Core/ContentHash.cpp
    Core/ResourceIO.hpp
    Core/ResourceIO.cpp
    Core/FrameArena.hpp
    Core/FrameArena.cpp
    Core/Error.hpp
    Core/Error.cpp
    Core/Diagnostics.hpp
    Core/Diagnostics.cpp
)
target_include_directories(PhysaraCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PhysaraCore PRIVATE spdlog bcrypt)
target_compile_features(PhysaraCore PUBLIC cxx_std_20)
set_target_properties(PhysaraCore PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraCore PRIVATE /W4 /WX /utf-8 /permissive-)

# Asset 使用 Core 的受控 I/O 和作业, 只在解码实现中消费 stb.
add_subdirectory(Asset)

# 独立图目标提供 CPU 声明和计划, 并通过公共 RHI 在借用的即时录制中执行.
add_subdirectory(Renderer/RenderGraph)

# Renderer 管理诊断 View 的颜色代和产品上传声明. 公开头使用图值, 因此传播 RenderGraph 依赖.
# Editor 与 Runtime 经此依赖消费公共图接口; Renderer 本身不反向依赖窗口或 UI.
add_library(PhysaraRenderer STATIC Renderer/View.hpp Renderer/View.cpp Renderer/FrameGraph.hpp Renderer/FrameGraph.cpp)
target_link_libraries(PhysaraRenderer PUBLIC PhysaraCore PhysaraRHI PhysaraRenderGraph)
target_compile_features(PhysaraRenderer PUBLIC cxx_std_20)
set_target_properties(PhysaraRenderer PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRenderer PRIVATE /W4 /WX /utf-8 /permissive-)

# Shader 目录拥有包编译库及生成入口, 由使用包的目标传入接口和源码.
add_subdirectory(Shader)
