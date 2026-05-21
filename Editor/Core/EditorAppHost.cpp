#include "EditorAppHost.hpp"

#include <Editor/Core/EditorApp.hpp>

namespace Physara::Editor
{
    class EditorAppHost::Impl final
    {
    public:
        EditorApp app{};
    };

    EditorAppHost::EditorAppHost()
        : m_Impl(std::make_unique<Impl>())
    {
    }

    EditorAppHost::~EditorAppHost() = default;

    EditorAppHost::EditorAppHost(EditorAppHost &&) noexcept = default;

    EditorAppHost &EditorAppHost::operator=(EditorAppHost &&) noexcept = default;

    void EditorAppHost::Init(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Platform::IInput *input)
    {
        m_Impl->app.Init(device, backend, input);
    }

    void EditorAppHost::Shutdown()
    {
        m_Impl->app.Shutdown();
    }

    void EditorAppHost::OnUIRender()
    {
        m_Impl->app.OnUIRender();
    }
}