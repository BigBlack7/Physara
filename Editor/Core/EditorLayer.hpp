#pragma once

#include <Engine/Core/Layer.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>

#include <Editor/Core/EditorApp.hpp>

namespace Physara::Editor
{
    class EditorLayer final : public Engine::Layer
    {
    public:
        explicit EditorLayer(RHI::IImGuiBackend *backend);

        void OnAttach() override;
        void OnDetach() override;
        void OnUIRender() override;

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        EditorApp m_App{};
    };
}