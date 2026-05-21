add_library(Backend STATIC)

target_sources(Backend PRIVATE
    RuntimeBackendFactory.cpp
    RuntimeBackendFactory.hpp
    OpenGL/OpenGLBuffer.cpp
    OpenGL/OpenGLBuffer.hpp
    OpenGL/OpenGLCommandList.cpp
    OpenGL/OpenGLCommandList.hpp
    OpenGL/OpenGLDevice.cpp
    OpenGL/OpenGLDevice.hpp
    OpenGL/OpenGLFramebuffer.cpp
    OpenGL/OpenGLFramebuffer.hpp
    OpenGL/OpenGLImGuiBackend.cpp
    OpenGL/OpenGLImGuiBackend.hpp
    OpenGL/OpenGLPipeline.cpp
    OpenGL/OpenGLPipeline.hpp
    OpenGL/OpenGLSampler.cpp
    OpenGL/OpenGLSampler.hpp
    OpenGL/OpenGLShader.cpp
    OpenGL/OpenGLShader.hpp
    OpenGL/OpenGLTexture.cpp
    OpenGL/OpenGLTexture.hpp
    OpenGL/OpenGLTypeMapping.hpp
)

target_include_directories(Backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Backend PUBLIC
    glad
    Engine
)

target_link_libraries(Backend PRIVATE
    Platform
    glfw3
    imgui
)