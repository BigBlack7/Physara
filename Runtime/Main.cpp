#include <memory>
#include <stdexcept>

#include <Engine/Core/Application.hpp>
#include <Engine/Core/Layer.hpp>
#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>
#include <Platform/Input/GLFWInput.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

namespace TODO
{
    // TODO: 替换为EditorLayer, 如Physara::Editor::EditorLayer
    class EditorLayerStub final : public Physara::Engine::Layer
    {
    };
}

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

        // TODO: RHIDevice接口与OpenGLDevice实现后在这里创建真实device
        Physara::RHI::RHIDevice *device = nullptr;
        Physara::RHI::IImGuiBackend *backend = nullptr;

        Physara::Engine::Application app;
        app.Init(window.get(), &input, device, nullptr);

        std::unique_ptr<Physara::Engine::Layer> editorLayer = std::make_unique<TODO::EditorLayerStub>();
        app.PushLayer(editorLayer.get());

        app.Run();
        app.Shutdown();

        window->Destroy();
    }
    catch (const std::exception &e)
    {
        PHYSARA_CORE_ERROR("Fatal exception: {}", e.what());
    }

    Physara::Engine::Log::Shutdown();
    return 0;
}