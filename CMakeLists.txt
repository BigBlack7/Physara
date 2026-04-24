cmake_minimum_required(VERSION 3.20)
project(Physara)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compiler options
if(MSVC)
    add_compile_options(/MP)
    add_compile_options(/utf-8)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

add_compile_definitions(ASSETS_PATH="${CMAKE_SOURCE_DIR}/Assets/")

# output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# add_subdirectory(Backend)
# add_subdirectory(Editor)
add_subdirectory(Engine)
add_subdirectory(Platform)
add_subdirectory(Runtime)
add_subdirectory(ThirdParty)