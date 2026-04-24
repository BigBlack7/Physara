#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

int main()
{
    Physara::Engine::Log::Init();
    try
    {
        Physara::Platform::FileSystem::Init(ASSETS_PATH);

        if (!Physara::Platform::FileSystem::Exists("Icons"))
        {
            PHYSARA_CORE_ERROR("Icons directory does not exist under Assets.");
        }
        else if (!Physara::Platform::FileSystem::IsDirectory("Icons"))
        {
            PHYSARA_CORE_ERROR("Icons exists but is not a directory.");
        }
        else
        {
            const auto& list = Physara::Platform::FileSystem::ListDirectory("Icons");
            PHYSARA_CORE_INFO("Icons entry count = {}", list.size());

            if (!list.empty())
            {
                PHYSARA_CORE_INFO("First entry = {}", list.front());
            }
            else
            {
                PHYSARA_CORE_WARN("Icons directory is empty.");
            }
        }
    }
    catch (const std::exception &e)
    {
        PHYSARA_CORE_ERROR("Exception: {}", e.what());
    }
    Physara::Engine::Log::Shutdown();
    return 0;
}