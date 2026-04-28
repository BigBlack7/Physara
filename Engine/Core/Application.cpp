#include "Application.hpp"

#include <vector>

#include <Engine/Core/Layer.hpp>
#include <Engine/Core/Log.hpp>
#include <Engine/Core/Time.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Platform/Input/IInput.hpp>
#include <Platform/Window/IWindow.hpp>

namespace Physara::Engine
{
    void Application::Init(Platform::IWindow *window, Platform::IInput *input, RHI::RHIDevice *device, RHI::IImGuiBackend *imguiBackend)
    {
        if (m_Initialized)
        {
            PHYSARA_CORE_WARN("Application::Init called more than once, ignored.");
            return;
        }

        if (window == nullptr || input == nullptr)
        {
            PHYSARA_CORE_ERROR("Application::Init failed: window/input is null.");
            return;
        }

        m_Window = window;
        m_Input = input;
        m_Device = device;
        m_ImGuiBackend = imguiBackend;

        if (m_Device == nullptr)
        {
            PHYSARA_CORE_WARN("Application initialized without RHIDevice. UI backend calls are skipped.");
        }
        if (m_ImGuiBackend == nullptr)
        {
            PHYSARA_CORE_WARN("Application initialized without ImGui backend. UI calls are skipped.");
        }

        m_Initialized = true;
    }

    void Application::Run()
    {
        if (!m_Initialized)
        {
            PHYSARA_CORE_ERROR("Application::Run called before Init.");
            return;
        }

        m_Running = true;

        while (m_Running && !m_Window->IsCloseRequested())
        {
            // 1) PollEvents
            m_Window->PollEvents();

            // 2) Input::BeginFrame
            m_Input->BeginFrame();

            // 3) Time::Tick
            Time::Tick();
            const float deltaTime = static_cast<float>(Time::GetDeltaTime());

            // 4) LayerStack::OnUpdate(deltaTime)
            for (auto it = m_LayerStack.Begin(); it != m_LayerStack.End(); ++it)
            {
                Layer *layer = *it;
                if (layer != nullptr)
                {
                    layer->OnUpdate(deltaTime);
                }
            }

            // 5) ImGui backend begin frame (platform + renderer)
            // TODO: IImGuiBackend实现后在这里调用BeginFrame()
            if (m_Device != nullptr)
            {
                // Reserved call site
            }
            if (m_ImGuiBackend != nullptr)
            {
                m_ImGuiBackend->BeginFrame();
            }

            // 6) LayerStack::OnUIRender
            for (auto it = m_LayerStack.Begin(); it != m_LayerStack.End(); ++it)
            {
                Layer *layer = *it;
                if (layer != nullptr)
                {
                    layer->OnUIRender();
                }
            }

            // 7) ImGui backend end frame + render draw data
            // TODO: IImGuiBackend实现后在这里调用EndFrame()/RenderDrawData()
            if (m_Device != nullptr)
            {
                // Reserved call site
            }
            if (m_ImGuiBackend != nullptr)
            {
                m_ImGuiBackend->EndFrame();
                m_ImGuiBackend->RenderDrawData();
            }

            // 8) Window::SwapBuffers
            m_Window->SwapBuffers();
        }
    }

    void Application::PushLayer(Layer *layer)
    {
        m_LayerStack.PushLayer(layer);
    }

    void Application::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        // 逆序退栈, 触发OnDetach
        std::vector<Layer *> layers;
        layers.reserve(static_cast<std::size_t>(m_LayerStack.End() - m_LayerStack.Begin()));
        for (auto it = m_LayerStack.Begin(); it != m_LayerStack.End(); ++it)
        {
            if (*it != nullptr)
            {
                layers.push_back(*it);
            }
        }

        for (auto it = layers.rbegin(); it != layers.rend(); ++it)
        {
            m_LayerStack.PopLayer(*it);
        }

        m_Running = false;
        m_Initialized = false;

        m_Device = nullptr;
        m_ImGuiBackend = nullptr;
        m_Input = nullptr;
        m_Window = nullptr;
    }
}