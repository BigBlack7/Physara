#pragma once

#include <cstdint>
#include <memory>

#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::RHI
{
    class RHIDevice;
    class RHIFramebuffer;
    class RHITexture;
}

namespace Physara::Engine
{
    class DeferredResources final
    {
    public:
        bool Ensure(
            RHI::RHIDevice &device,
            std::uint32_t width,
            std::uint32_t height,
            RHI::RHITexture &sceneHDR,
            RHI::RHITexture &sceneDepth);
        void Reset();

        [[nodiscard]] bool IsReady() const;
        [[nodiscard]] RHI::RHIFramebuffer *GetGBufferFramebuffer() const { return m_GBufferFramebuffer.get(); }
        [[nodiscard]] RHI::RHIFramebuffer *GetLightingFramebuffer() const { return m_LightingFramebuffer.get(); }
        [[nodiscard]] RHI::RHITexture *GetBaseColor() const { return m_BaseColor.get(); }
        [[nodiscard]] RHI::RHITexture *GetNormal() const { return m_Normal.get(); }
        [[nodiscard]] RHI::RHITexture *GetMaterial() const { return m_Material.get(); }
        [[nodiscard]] RHI::RHITexture *GetEmissive() const { return m_Emissive.get(); }
        [[nodiscard]] const RHI::RHIRenderPassDesc &GetGBufferRenderPassDesc() const { return m_GBufferRenderPassDesc; }
        [[nodiscard]] const RHI::RHIRenderPassDesc &GetLightingClearRenderPassDesc() const { return m_LightingClearRenderPassDesc; }
        [[nodiscard]] const RHI::RHIRenderPassDesc &GetLightingLoadRenderPassDesc() const { return m_LightingLoadRenderPassDesc; }

    private:
        std::unique_ptr<RHI::RHITexture> m_BaseColor{};
        std::unique_ptr<RHI::RHITexture> m_Normal{};
        std::unique_ptr<RHI::RHITexture> m_Material{};
        std::unique_ptr<RHI::RHITexture> m_Emissive{};
        std::unique_ptr<RHI::RHIFramebuffer> m_GBufferFramebuffer{};
        std::unique_ptr<RHI::RHIFramebuffer> m_LightingFramebuffer{};
        RHI::RHIRenderPassDesc m_GBufferRenderPassDesc{};
        RHI::RHIRenderPassDesc m_LightingClearRenderPassDesc{};
        RHI::RHIRenderPassDesc m_LightingLoadRenderPassDesc{};
        std::uint32_t m_Width{0};
        std::uint32_t m_Height{0};
        RHI::RHITexture *m_SceneHDR{nullptr};
        RHI::RHITexture *m_SceneDepth{nullptr};
    };
}