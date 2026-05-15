#pragma once

#include <Engine/Core/Layer.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Platform/Input/IInput.hpp>

#include <Editor/Core/EditorApp.hpp>

namespace Physara::Editor
{
    class EditorLayer final : public Engine::Layer
    {
    public:
        EditorLayer(RHI::RHIDevice *device, RHI::IImGuiBackend *backend, Platform::IInput *input);

        void OnAttach() override;
        void OnDetach() override;
        void OnUIRender() override;

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        RHI::RHIDevice *m_Device{nullptr};
        Platform::IInput *m_Input{nullptr};
        EditorApp m_App{};
    };
}