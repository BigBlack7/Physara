#include <memory>
#include <stdexcept>

#include <Backend/OpenGL/OpenGLDevice.hpp>
#include <Backend/OpenGL/OpenGLImGuiBackend.hpp>
#include <Editor/Core/EditorLayer.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Core/Layer.hpp>
#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>
#include <Platform/Input/GLFWInput.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

int main()
{
    Physara::Engine::Log::Init();
    try
    {
        Physara::Platform::FileSystem::Init(ASSETS_PATH);

        std::unique_ptr<Physara::Platform::IWindow> window = std::make_unique<Physara::Platform::GLFWWindowOpenGL>();
        if (!window->Create("Physara", 1960, 1080))
        {
            throw std::runtime_error("Failed to create window.");
        }

        window->SetResizeCallback([](int w, int h)
                                  { PHYSARA_CORE_INFO("Window resized: {} x {}", w, h); });

        PHYSARA_CORE_INFO("Window created: {} x {}", window->GetWidth(), window->GetHeight());

        auto *nativeWindow = static_cast<GLFWwindow *>(window->GetNativeHandle());
        if (nativeWindow == nullptr)
        {
            throw std::runtime_error("Window native handle is null.");
        }

        Physara::Platform::GLFWInput input(nativeWindow);

        auto device = std::make_unique<Physara::RHI::OpenGLDevice>();
        if (!device->Init(nativeWindow))
        {
            throw std::runtime_error("Failed to initialize OpenGL device.");
        }

        auto imguiBackend = std::make_unique<Physara::RHI::OpenGLImGuiBackend>();
        if (!imguiBackend->Initialize(device.get(), nativeWindow))
        {
            throw std::runtime_error("Failed to initialize OpenGL ImGui backend.");
        }

        Physara::Engine::Application app;
        app.Init(window.get(), &input, device.get());

        std::unique_ptr<Physara::Engine::Layer> editorLayer =
            std::make_unique<Physara::Editor::EditorLayer>(imguiBackend.get());
        app.PushLayer(editorLayer.get());

        app.Run();
        app.Shutdown();

        imguiBackend->Shutdown();
        device->Shutdown();
        window->Destroy();
    }
    catch (const std::exception &e)
    {
        PHYSARA_CORE_ERROR("Fatal exception: {}", e.what());
    }

    Physara::Engine::Log::Shutdown();
    return 0;
}
