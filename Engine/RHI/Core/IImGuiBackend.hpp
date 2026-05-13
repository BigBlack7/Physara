#pragma once

#include <cstdint>

namespace Physara::RHI
{
    class RHIDevice;
    class RHITexture;

    using ImGuiTextureHandle = std::uint64_t;

    class IImGuiBackend
    {
    public:
        virtual ~IImGuiBackend() = default;

        virtual bool Initialize(RHIDevice *device, void *windowHandle) = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void RenderDrawData() = 0;
        virtual ImGuiTextureHandle CreateTextureRGBA(std::uint32_t width, std::uint32_t height, const void *pixels) = 0;
        virtual void DestroyTexture(ImGuiTextureHandle texture) = 0;
        virtual ImGuiTextureHandle GetTextureHandle(RHITexture *texture) = 0;
        virtual void Shutdown() = 0;
    };
}