# Core 只拥有 CPU 基础机制; 日志后端为私有依赖, 不向消费者泄漏 spdlog 头.
add_library(PhysaraCore STATIC
    Core/Identity.hpp
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
