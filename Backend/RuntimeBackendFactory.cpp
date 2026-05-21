#include "RuntimeBackendFactory.hpp"

#include <stdexcept>

#include <Backend/OpenGL/OpenGLDevice.hpp>
#include <Backend/OpenGL/OpenGLImGuiBackend.hpp>
#include <Platform/Input/GLFWInput.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

namespace Physara::RHI
{
    namespace RuntimeBackendFactoryDetail
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

        RuntimeBackendFactoryDetail::ThrowUnsupportedBackend();
    }

    std::unique_ptr<Platform::IInput> CreateRuntimeInput(GraphicsBackend backend, void *nativeWindow)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<Platform::GLFWInput>(nativeWindow);
        }

        RuntimeBackendFactoryDetail::ThrowUnsupportedBackend();
    }

    std::unique_ptr<RHIDevice> CreateRuntimeDevice(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<OpenGLDevice>();
        }

        RuntimeBackendFactoryDetail::ThrowUnsupportedBackend();
    }

    std::unique_ptr<IImGuiBackend> CreateRuntimeImGuiBackend(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::OpenGL:
            return std::make_unique<OpenGLImGuiBackend>();
        }

        RuntimeBackendFactoryDetail::ThrowUnsupportedBackend();
    }
}