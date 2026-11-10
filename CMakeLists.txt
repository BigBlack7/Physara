cmake_minimum_required(VERSION 3.24)
project(Physara VERSION 2.0.0 LANGUAGES C CXX)

# 当前第三方源码按 Windows x64/MSVC 组织, 其他平台接入时再补齐原生实现.
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Physara currently supports Windows x64 with MSVC.")
endif()

# 所有编译产物都进入构建目录; Debug/Release 使用生成器的常规配置.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 先定义固定第三方源码的 target, 供第一方模块按需链接.
add_subdirectory(ThirdParty)

# 通用 RHI 与原生 GL 分开. 单独消费通用 RHI 时只需进入 RHI 目录.
add_subdirectory(RHI)
add_subdirectory(RHI/OpenGL)
# Engine 提供 Core 与 Shader. Platform 先建立宿主合同, Editor 再消费其窗口和输入值.
add_subdirectory(Engine)
add_subdirectory(Platform)
add_subdirectory(Editor)
# 最后由 Runtime 装配各模块的唯一产品入口.
add_subdirectory(Runtime)
