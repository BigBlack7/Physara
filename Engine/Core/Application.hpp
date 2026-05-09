#pragma once

#include <Engine/Core/LayerStack.hpp>

namespace Physara::Platform
{
    class IWindow;
    class IInput;
}

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class Layer;

    class Application final
    {
    public:
        Application() = default;
        ~Application() = default;

        void Init(Platform::IWindow *window, Platform::IInput *input, RHI::RHIDevice *device);
        void Run();
        void PushLayer(Layer *layer);
        void Shutdown();

    private:
        Platform::IWindow *m_Window{nullptr};
        Platform::IInput *m_Input{nullptr};
        RHI::RHIDevice *m_Device{nullptr};

        LayerStack m_LayerStack{};

        bool m_Initialized{false};
        bool m_Running{false};
    };
}