#include "RuntimeBackendFactory.hpp"

#include <stdexcept>

#include <Backend/OpenGL/OpenGLDevice.hpp>
#include <Backend/OpenGL/OpenGLImGuiBackend.hpp>
#include <Platform/Input/GLFWInput.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

namespace Physara::RHI
{
    namespace
    {
        [[noreturn]] void ThrowUnsupportedBackend()
        {
            throw std::runtime_error("Unsupported runtime graphics backend.");
        }
    }

    std::unique_ptr<Platform::IWindow> CreateRuntimeWindow(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<Platform::GLFWWindowOpenGL>();
        }

        ThrowUnsupportedBackend();
    }

    std::unique_ptr<Platform::IInput> CreateRuntimeInput(GraphicsBackend backend, void *nativeWindow)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<Platform::GLFWInput>(nativeWindow);
        }

        ThrowUnsupportedBackend();
    }

    std::unique_ptr<RHIDevice> CreateRuntimeDevice(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<OpenGLDevice>();
        }

        ThrowUnsupportedBackend();
    }

    std::unique_ptr<IImGuiBackend> CreateRuntimeImGuiBackend(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<OpenGLImGuiBackend>();
        }

        ThrowUnsupportedBackend();
    }
}