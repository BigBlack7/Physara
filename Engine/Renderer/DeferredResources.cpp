#include "DeferredResources.hpp"

#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>

namespace Physara::Engine
{
    namespace DeferredResourcesDetail
    {
        std::unique_ptr<RHI::RHITexture> CreateTarget(
            RHI::RHIDevice &device,
            std::uint32_t width,
            std::uint32_t height,
            RHI::TextureFormat format)
        {
            RHI::RHITextureDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.format = format;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::RenderTarget;
            desc.mipLevels = 1u;
            desc.arrayLayers = 1u;
            desc.samples = 1u;
            return device.CreateTexture(desc);
        }
    }

    bool DeferredResources::Ensure(
        RHI::RHIDevice &device,
        std::uint32_t width,
        std::uint32_t height,
        RHI::RHITexture &sceneHDR,
        RHI::RHITexture &sceneDepth)
    {
        if (IsReady() && m_Width == width && m_Height == height &&
            m_SceneHDR == &sceneHDR && m_SceneDepth == &sceneDepth)
        {
            return true;
        }

        Reset();
        m_Width = width;
        m_Height = height;
        m_SceneHDR = &sceneHDR;
        m_SceneDepth = &sceneDepth;
        m_BaseColor = DeferredResourcesDetail::CreateTarget(device, width, height, RHI::TextureFormat::RGBA8);
        m_Normal = DeferredResourcesDetail::CreateTarget(device, width, height, RHI::TextureFormat::RG16F);
        m_Material = DeferredResourcesDetail::CreateTarget(device, width, height, RHI::TextureFormat::RGBA8);
        m_Emissive = DeferredResourcesDetail::CreateTarget(device, width, height, RHI::TextureFormat::RGBA16F);
        if (m_BaseColor == nullptr || m_Normal == nullptr || m_Material == nullptr || m_Emissive == nullptr)
        {
            Reset();
            return false;
        }

        m_GBufferRenderPassDesc = {};
        m_GBufferRenderPassDesc.colorAttachments = {
            {RHI::TextureFormat::RGBA8, RHI::LoadOp::Clear, RHI::StoreOp::Store, 1u},
            {RHI::TextureFormat::RG16F, RHI::LoadOp::Clear, RHI::StoreOp::Store, 1u},
            {RHI::TextureFormat::RGBA8, RHI::LoadOp::Clear, RHI::StoreOp::Store, 1u},
            {RHI::TextureFormat::RGBA16F, RHI::LoadOp::Clear, RHI::StoreOp::Store, 1u}};
        m_GBufferRenderPassDesc.depthAttachment = {
            RHI::TextureFormat::Depth24Stencil8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u};
        m_GBufferRenderPassDesc.hasDepth = true;

        m_LightingClearRenderPassDesc = {};
        m_LightingClearRenderPassDesc.colorAttachments.push_back({
            RHI::TextureFormat::RGBA16F,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_LightingLoadRenderPassDesc = m_LightingClearRenderPassDesc;
        m_LightingLoadRenderPassDesc.colorAttachments[0].loadOp = RHI::LoadOp::Load;

        RHI::RHIFramebufferDesc gBufferFramebufferDesc{};
        gBufferFramebufferDesc.colorAttachments = {
            m_BaseColor.get(),
            m_Normal.get(),
            m_Material.get(),
            m_Emissive.get()};
        gBufferFramebufferDesc.depthAttachment = &sceneDepth;
        gBufferFramebufferDesc.width = width;
        gBufferFramebufferDesc.height = height;
        gBufferFramebufferDesc.renderPassDesc = &m_GBufferRenderPassDesc;
        m_GBufferFramebuffer = device.CreateFramebuffer(gBufferFramebufferDesc);

        RHI::RHIFramebufferDesc lightingFramebufferDesc{};
        lightingFramebufferDesc.colorAttachments.push_back(&sceneHDR);
        lightingFramebufferDesc.width = width;
        lightingFramebufferDesc.height = height;
        lightingFramebufferDesc.renderPassDesc = &m_LightingClearRenderPassDesc;
        m_LightingFramebuffer = device.CreateFramebuffer(lightingFramebufferDesc);
        if (!IsReady())
        {
            Reset();
            return false;
        }
        return true;
    }

    void DeferredResources::Reset()
    {
        m_LightingFramebuffer.reset();
        m_GBufferFramebuffer.reset();
        m_Emissive.reset();
        m_Material.reset();
        m_Normal.reset();
        m_BaseColor.reset();
        m_Width = 0u;
        m_Height = 0u;
        m_SceneHDR = nullptr;
        m_SceneDepth = nullptr;
    }

    bool DeferredResources::IsReady() const
    {
        return m_BaseColor != nullptr && m_Normal != nullptr && m_Material != nullptr &&
               m_Emissive != nullptr && m_GBufferFramebuffer != nullptr && m_LightingFramebuffer != nullptr;
    }
}