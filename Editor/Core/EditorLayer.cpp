#include "EditorLayer.hpp"

namespace Physara::Editor
{
    EditorLayer::EditorLayer(RHI::RHIDevice *device, RHI::IImGuiBackend *backend)
        : m_Backend(backend), m_Device(device)
    {
    }

    void EditorLayer::OnAttach()
    {
        m_App.Init(m_Device, m_Backend);
    }

    void EditorLayer::OnDetach()
    {
        m_App.Shutdown();
    }

    void EditorLayer::OnUIRender()
    {
        m_App.OnUIRender();
    }
}