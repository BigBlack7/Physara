file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(Engine STATIC ${ENGINE_SOURCES})

target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(Engine PUBLIC PHYSARA_DEBUG)

target_link_libraries(Engine PUBLIC
    Platform
    entt
    spdlog
    nlohmann
    tinygltf
    tinyexr
)