add_library(Platform STATIC)

target_sources(Platform PRIVATE
    FileSystem/FileSystem.cpp
    FileSystem/FileSystem.hpp
    Input/GLFWInput.cpp
    Input/GLFWInput.hpp
    Input/IInput.hpp
    Input/KeyCodes.hpp
    Window/GLFWWindowContext.hpp
    Window/GLFWWindowOpenGL.cpp
    Window/GLFWWindowOpenGL.hpp
    Window/IWindow.hpp
)

target_include_directories(Platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Platform PUBLIC
    glm
)

target_link_libraries(Platform PRIVATE
    stb
    glfw3
)
