#include "EditorLayer.hpp"

namespace Physara::Editor
{
    EditorLayer::EditorLayer(RHI::IImGuiBackend *backend)
        : m_Backend(backend)
    {
    }

    void EditorLayer::OnAttach()
    {
        m_App.Init(m_Backend);
    }

    void EditorLayer::OnUIRender()
    {
        m_App.OnUIRender();
    }
}