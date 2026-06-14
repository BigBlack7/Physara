#include <memory>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include <Backend/RuntimeBackendFactory.hpp>
#include <Editor/Core/EditorAppHost.hpp>
#include <Engine/Core/Log.hpp>
#include <Engine/Core/Time.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Platform/FileSystem/FileSystem.hpp>
#include <Platform/Input/IInput.hpp>
#include <Platform/Window/IWindow.hpp>

namespace MainDetail
{
    bool IsTruthy(std::string_view value)
    {
        return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
    }

    bool ReadVSyncSetting()
    {
        const char *value = std::getenv("PHYSARA_VSYNC");
        return value != nullptr && IsTruthy(value);
    }
}

int main()
{
    Physara::Engine::Log::Init();

    int exitCode = 0;

    std::unique_ptr<Physara::Platform::IWindow> window;
    std::unique_ptr<Physara::Platform::IInput> input;
    std::unique_ptr<Physara::RHI::RHIDevice> device;
    std::unique_ptr<Physara::RHI::IImGuiBackend> imguiBackend;
    std::unique_ptr<Physara::Editor::EditorAppHost> editorApp;

    try
    {
        Physara::Platform::FileSystem::Init(ASSETS_PATH);

        constexpr auto graphicsBackend = Physara::RHI::GraphicsBackend::OpenGL;
        window = Physara::RHI::CreateRuntimeWindow(graphicsBackend);
        if (!window->Create("Physara", 1960, 1080))
        {
            throw std::runtime_error("Failed to create window.");
        }

        void *nativeWindow = window->GetNativeHandle();
        if (nativeWindow == nullptr)
        {
            throw std::runtime_error("Window native handle is null.");
        }
        PHYSARA_CORE_INFO("Window created: {} x {}", window->GetWidth(), window->GetHeight());

        input = Physara::RHI::CreateRuntimeInput(graphicsBackend, nativeWindow);

        device = Physara::RHI::CreateRuntimeDevice(graphicsBackend);
        if (!device->Init(nativeWindow))
        {
            throw std::runtime_error("Failed to initialize graphics device.");
        }

        const bool vsyncEnabled = MainDetail::ReadVSyncSetting();
        window->SetVSync(vsyncEnabled);
        PHYSARA_CORE_INFO("VSync {}", vsyncEnabled ? "enabled" : "disabled");

        imguiBackend = Physara::RHI::CreateRuntimeImGuiBackend(graphicsBackend);
        PHYSARA_CORE_INFO("Initializing ImGui backend...");
        if (!imguiBackend->Initialize(device.get(), nativeWindow))
        {
            throw std::runtime_error("Failed to initialize graphics ImGui backend.");
        }
        PHYSARA_CORE_INFO("ImGui backend initialized.");

        PHYSARA_CORE_INFO("Creating editor app host...");
        editorApp = std::make_unique<Physara::Editor::EditorAppHost>();
        PHYSARA_CORE_INFO("Initializing editor app...");
        editorApp->Init(device.get(), imguiBackend.get(), input.get());
        PHYSARA_CORE_INFO("Editor app initialized.");

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