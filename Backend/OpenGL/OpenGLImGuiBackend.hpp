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
        ImGuiTextureHandle GetTextureHandle(RHITexture *texture) override;
        [[nodiscard]] ImGuiRenderStatistics GetLastRenderStatistics() const override { return m_LastRenderStatistics; }
        void Shutdown() override;

    private:
        RHIDevice *m_Device{nullptr};
        ImGuiRenderStatistics m_LastRenderStatistics{};
        bool m_Initialized{false};
        bool m_OwnsContext{false};
    };
}