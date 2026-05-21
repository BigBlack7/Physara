#pragma once

#include <memory>

namespace Physara::Platform
{
    class IInput;
    class IWindow;
}

namespace Physara::RHI
{
    class IImGuiBackend;
    class RHIDevice;

    enum class GraphicsBackend
    {
        OpenGL
    };

    [[nodiscard]] std::unique_ptr<Platform::IWindow> CreateRuntimeWindow(GraphicsBackend backend);
    [[nodiscard]] std::unique_ptr<Platform::IInput> CreateRuntimeInput(GraphicsBackend backend, void *nativeWindow);
    [[nodiscard]] std::unique_ptr<RHIDevice> CreateRuntimeDevice(GraphicsBackend backend);
    [[nodiscard]] std::unique_ptr<IImGuiBackend> CreateRuntimeImGuiBackend(GraphicsBackend backend);
}