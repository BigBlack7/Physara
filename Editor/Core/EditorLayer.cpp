#include "EditorLayer.hpp"

namespace Physara::Editor
{
    EditorLayer::EditorLayer(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Platform::IInput *input)
        : m_Backend(backend), m_Device(device), m_Input(input)
    {
    }

    void EditorLayer::OnAttach()
    {
        m_App.Init(m_Device, m_Backend, m_Input);
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