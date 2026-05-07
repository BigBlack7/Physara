#pragma once

#include <Engine/RHI/Core/IImGuiBackend.hpp>

namespace Physara::RHI
{
    class OpenGLImGuiBackend final : public IImGuiBackend
    {
    public:
        OpenGLImGuiBackend() = default;
        ~OpenGLImGuiBackend() override = default;

        bool Initialize(RHIDevice *device, void *windowHandle) override;
        void BeginFrame() override;
        void EndFrame() override;
        void RenderDrawData() override;
        void Shutdown() override;
    };
}
