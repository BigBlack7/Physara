#pragma once

#include <Engine/RHI/Core/IImGuiBackend.hpp>

namespace Physara::RHI
{
    class OpenGLImGuiBackend final : public IImGuiBackend
    {
    public:
        OpenGLImGuiBackend() = default;
        ~OpenGLImGuiBackend() override;

        bool Initialize(RHIDevice *device, void *windowHandle) override;
        void BeginFrame() override;
        void EndFrame() override;
        void RenderDrawData() override;
        ImGuiTextureHandle CreateTextureRGBA(std::uint32_t width, std::uint32_t height, const void *pixels) override;
        void DestroyTexture(ImGuiTextureHandle texture) override;
        void Shutdown() override;

    private:
        bool m_Initialized{false};
        bool m_OwnsContext{false};
    };
}