#include <memory>
#include <stdexcept>

#include <Backend/OpenGL/OpenGLDevice.hpp>
#include <Backend/OpenGL/OpenGLImGuiBackend.hpp>
#include <Editor/Core/EditorApp.hpp>
#include <Engine/Core/Log.hpp>
#include <Engine/Core/Time.hpp>
#include <Platform/FileSystem/FileSystem.hpp>
#include <Platform/Input/GLFWInput.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

int main()
{
    Physara::Engine::Log::Init();

    int exitCode = 0;

    std::unique_ptr<Physara::Platform::IWindow> window;
    std::unique_ptr<Physara::Platform::IInput> input;
    std::unique_ptr<Physara::RHI::RHIDevice> device;
    std::unique_ptr<Physara::RHI::IImGuiBackend> imguiBackend;
    std::unique_ptr<Physara::Editor::EditorApp> editorApp;

    try
    {
        Physara::Platform::FileSystem::Init(ASSETS_PATH);

        window = std::make_unique<Physara::Platform::GLFWWindowOpenGL>();
        if (!window->Create("Physara", 1960, 1080))
        {
            throw std::runtime_error("Failed to create window.");
        }

        window->SetResizeCallback([](int width, int height)
                                  { PHYSARA_CORE_INFO("Window resized: {} x {}", width, height); });

        void *nativeWindow = window->GetNativeHandle();
        if (nativeWindow == nullptr)
        {
            throw std::runtime_error("Window native handle is null.");
        }

        PHYSARA_CORE_INFO("Window created: {} x {}", window->GetWidth(), window->GetHeight());

        input = std::make_unique<Physara::Platform::GLFWInput>(nativeWindow);

        device = std::make_unique<Physara::RHI::OpenGLDevice>();
        if (!device->Init(nativeWindow))
        {
            throw std::runtime_error("Failed to initialize OpenGL device.");
        }

        window->SetVSync(true);

        imguiBackend = std::make_unique<Physara::RHI::OpenGLImGuiBackend>();
        if (!imguiBackend->Initialize(device.get(), nativeWindow))
        {
            throw std::runtime_error("Failed to initialize OpenGL ImGui backend.");
        }

        editorApp = std::make_unique<Physara::Editor::EditorApp>();
        editorApp->Init(device.get(), imguiBackend.get(), input.get());

        while (!window->IsCloseRequested())
        {
            window->PollEvents();
            input->BeginFrame();
            Physara::Engine::Time::Tick();
            editorApp->OnUIRender();
            window->SwapBuffers();
        }
    }
    catch (const std::exception &e)
    {
        PHYSARA_CORE_ERROR("Fatal exception: {}", e.what());
        exitCode = 1;
    }

    if (editorApp != nullptr)
    {
        editorApp->Shutdown();
    }
    editorApp.reset();

    if (imguiBackend != nullptr)
    {
        imguiBackend->Shutdown();
    }
    imguiBackend.reset();

    if (device != nullptr)
    {
        device->WaitIdle();
        device->Shutdown();
    }
    device.reset();

    input.reset();

    if (window != nullptr)
    {
        window->Destroy();
    }
    window.reset();

    Physara::Engine::Log::Shutdown();
    return exitCode;
}