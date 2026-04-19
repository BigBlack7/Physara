# glfw
add_library(glfw3 STATIC IMPORTED GLOBAL)
set_target_properties(glfw3 PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib/glfw3.lib"
)
target_include_directories(glfw3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/glfw/include
)

# glad
add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

# glm
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/glm/include
)

# entt
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/entt/include
)

# imgui
add_library(imgui STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/GraphEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src/imgui/ImSequencer.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/src
)
target_link_libraries(imgui PRIVATE glfw3 glad)

# nlohmann json
add_library(nlohmann INTERFACE)
target_include_directories(nlohmann INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/include
)

# tiny gltf
add_library(tinygltf tinygltf/src/tinygltf/tinygltf_v3_impl.cpp)
target_include_directories(tinygltf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf/src
)