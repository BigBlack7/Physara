#pragma once

namespace Physara::RHI
{
    class RHIDevice;

    class IImGuiBackend
    {
    public:
        virtual ~IImGuiBackend() = default;

        virtual bool Initialize(RHIDevice *device, void *windowHandle) = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void RenderDrawData() = 0;
        virtual void Shutdown() = 0;
    };
}