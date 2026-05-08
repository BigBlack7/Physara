#pragma once

#include <cstdint>

#include <Engine/RHI/Core/IImGuiBackend.hpp>

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class EditorApp
    {
    public:
        void Init(RHI::IImGuiBackend *backend);
        void OnUIRender();

    private:
        void InitDefaultLayout();

        RHI::IImGuiBackend *m_Backend{nullptr};
        EditorContext m_Context{};

        bool m_LayoutInitialized{false};
        std::uint32_t m_DockspaceId{0};
    };
}