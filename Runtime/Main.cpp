#include <memory>

#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>
#include <Platform/Window/GLFWWindowOpenGL.hpp>

int main()
{
    Physara::Engine::Log::Init();
    try
    {
        Physara::Platform::FileSystem::Init(ASSETS_PATH);

        std::unique_ptr<Physara::Platform::IWindow> window = std::make_unique<Physara::Platform::GLFWWindowOpenGL>();
        window->Create("Physara", 1960, 1080);
        window->SetResizeCallback([](int w, int h)
                                 {
                                     PHYSARA_CORE_INFO("Window resized: {} x {}", w, h);
                                     // End
                                 });
        PHYSARA_CORE_INFO("Window created: {} x {}", window->GetWidth(), window->GetHeight());

        while (!window->IsCloseRequested())
        {
            window->PollEvents();

            // 当前阶段仅验证窗口生命周期与事件循环, 不做任何GL调用
            // 等OpenGLDevice::Init(window.GetNativeHandle())接入后, 再在循环里执行SwapBuffers
            window->SwapBuffers();
        }

        window->Destroy();
    }
    catch (const std::exception &e)
    {
        PHYSARA_CORE_ERROR("Fatal exception: {}", e.what());
    }
    Physara::Engine::Log::Shutdown();
    return 0;
}