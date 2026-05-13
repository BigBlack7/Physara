#include "Renderer.hpp"

#include <algorithm>
#include <vector>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

namespace Physara::Engine
{
    Renderer::Renderer(RHI::RHIDevice *device)
    {
        Initialize(device);
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    void Renderer::Initialize(RHI::RHIDevice *device)
    {
        Shutdown();

        m_Device = device;
        m_RenderPassDesc = {};
        m_RenderPassDesc.colorAttachments.push_back({
            RHI::TextureFormat::RGBA8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_RenderPassDesc.hasDepth = false;

        if (m_Device == nullptr)
        {
            PHYSARA_CORE_WARN("Renderer initialized without RHIDevice.");
        }
    }

    void Renderer::Shutdown()
    {
        m_Framebuffer.reset();
        m_SceneColor.reset();
        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
        m_Device = nullptr;
    }

    void Renderer::ResizeViewport(std::uint32_t width, std::uint32_t height)
    {
        width = std::max(width, 1u);
        height = std::max(height, 1u);

        if (m_ViewportWidth == width && m_ViewportHeight == height && HasValidRenderTarget())
        {
            return;
        }

        m_ViewportWidth = width;
        m_ViewportHeight = height;
        RecreateRenderTarget();
    }

    void Renderer::RenderClear()
    {
        if (m_Device == nullptr || !HasValidRenderTarget())
        {
            return;
        }

        RHI::RHICommandList *commandList = m_Device->GetCommandList();
        if (commandList == nullptr)
        {
            PHYSARA_CORE_WARN("Renderer::RenderClear skipped because command list is null.");
            return;
        }

        commandList->BeginDebugLabel("Renderer.ClearSceneColor");
        commandList->SetViewport(0.f, 0.f, static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
        commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
        commandList->BeginRenderPass(m_Framebuffer.get(), m_RenderPassDesc, std::vector<glm::vec4>{m_ClearColor});
        commandList->EndRenderPass();
        commandList->EndDebugLabel();

        m_Device->SubmitCommandList();
    }

    bool Renderer::HasValidRenderTarget() const
    {
        return m_SceneColor != nullptr && m_Framebuffer != nullptr && m_ViewportWidth > 0 && m_ViewportHeight > 0;
    }

    void Renderer::RecreateRenderTarget()
    {
        m_Framebuffer.reset();
        m_SceneColor.reset();

        if (m_Device == nullptr || m_ViewportWidth == 0 || m_ViewportHeight == 0)
        {
            return;
        }

        RHI::RHITextureDesc sceneColorDesc{};
        sceneColorDesc.width = m_ViewportWidth;
        sceneColorDesc.height = m_ViewportHeight;
        sceneColorDesc.format = RHI::TextureFormat::RGBA8;
        sceneColorDesc.dimension = RHI::TextureDimension::Tex2D;
        sceneColorDesc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::RenderTarget;
        sceneColorDesc.mipLevels = 1;
        sceneColorDesc.arrayLayers = 1;
        sceneColorDesc.samples = 1;

        m_SceneColor = m_Device->CreateTexture(sceneColorDesc);
        if (m_SceneColor == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneColor render target.");
            return;
        }

        RHI::RHIFramebufferDesc framebufferDesc{};
        framebufferDesc.colorAttachments.push_back(m_SceneColor.get());
        framebufferDesc.width = m_ViewportWidth;
        framebufferDesc.height = m_ViewportHeight;
        framebufferDesc.renderPassDesc = &m_RenderPassDesc;

        m_Framebuffer = m_Device->CreateFramebuffer(framebufferDesc);
        if (m_Framebuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneColor framebuffer.");
            m_SceneColor.reset();
        }
    }
}