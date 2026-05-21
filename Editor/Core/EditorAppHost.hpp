#pragma once

#include <memory>

namespace Physara::Platform
{
    class IInput;
}

namespace Physara::RHI
{
    class IImGuiBackend;
    class RHIDevice;
}

namespace Physara::Editor
{
    class EditorAppHost final
    {
    public:
        EditorAppHost();
        ~EditorAppHost();

        EditorAppHost(const EditorAppHost &) = delete;
        EditorAppHost &operator=(const EditorAppHost &) = delete;
        EditorAppHost(EditorAppHost &&) noexcept;
        EditorAppHost &operator=(EditorAppHost &&) noexcept;

        void Init(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Platform::IInput *input);
        void Shutdown();
        void OnUIRender();

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}