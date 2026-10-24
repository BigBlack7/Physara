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

add_subdirectory(ThirdParty)

# 各模块在首次实现时声明自己的源文件, C++20, 警告和链接依赖.
# 当前目录尚无业务实现, 因此不创建空库或不可运行的程序.
add_subdirectory(RHI)
add_subdirectory(Platform)
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Runtime)
