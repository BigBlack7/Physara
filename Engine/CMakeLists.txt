# Engine 在此装配 Core, 独立图声明, View 和 Shader 目标.
# Core 提供 CPU 基础机制. FrameArena 的复用时机由 Runtime 决定, 日志后端 spdlog 私有链接.
add_library(PhysaraCore STATIC
    Core/Identity.hpp
    Core/FrameArena.hpp
    Core/FrameArena.cpp
    Core/Error.hpp
    Core/Error.cpp
    Core/Diagnostics.hpp
    Core/Diagnostics.cpp
)
target_include_directories(PhysaraCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PhysaraCore PRIVATE spdlog)
target_compile_features(PhysaraCore PUBLIC cxx_std_20)
set_target_properties(PhysaraCore PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraCore PRIVATE /W4 /WX /utf-8 /permissive-)

# 图声明先作为独立目标交付, 供 CPU 消费者验证资源版本和 Pass 权限.
add_subdirectory(Renderer/RenderGraph)

# Renderer 管理诊断 View 的颜色代. 公开头使用 Core 身份与通用 RHI 句柄, 因此传播两个目标.
add_library(PhysaraRenderer STATIC Renderer/View.hpp Renderer/View.cpp)
target_link_libraries(PhysaraRenderer PUBLIC PhysaraCore PhysaraRHI)
target_compile_features(PhysaraRenderer PUBLIC cxx_std_20)
set_target_properties(PhysaraRenderer PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRenderer PRIVATE /W4 /WX /utf-8 /permissive-)

# Shader 目录拥有包编译库及生成入口, 由使用包的目标传入接口和源码.
add_subdirectory(Shader)
