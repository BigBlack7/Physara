#include <Engine/Core/Log.hpp>

int main()
{
    Physara::Engine::Log::Init();

    PHYSARA_CORE_WARN("Renderer fallback to default material. id={}", 42);
    PHYSARA_CORE_ERROR("Shader compile failed: {}", "pbr.frag");

    PHYSARA_CORE_TRACE("Frame dt = {} ms", 16.67f);
    PHYSARA_CORE_INFO("Selected entity id={}", 1001);

    PHYSARA_CORE_FATAL("RHI init failed: {}", "OpenGL 4.6 unsupported");

    Physara::Engine::Log::Shutdown();
    return 0;
}