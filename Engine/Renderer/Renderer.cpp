#include "Renderer.hpp"

#include <algorithm>
#include <vector>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/Scene/Scene.hpp>

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
        m_ShaderLibrary.SetDevice(device);
        m_PipelineStateCache.SetDevice(device);
        m_RenderPassDesc = {};
        m_RenderPassDesc.colorAttachments.push_back({
            RHI::TextureFormat::RGBA8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_RenderPassDesc.depthAttachment = {
            RHI::TextureFormat::Depth24Stencil8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u};
        m_RenderPassDesc.hasDepth = true;

        if (m_Device == nullptr)
        {
            PHYSARA_CORE_WARN("Renderer initialized without RHIDevice.");
        }
    }

    void Renderer::Shutdown()
    {
        m_RenderGraph.Reset();
        m_Framebuffer.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_ShaderLibrary.SetDevice(nullptr);
        m_PipelineStateCache.SetDevice(nullptr);
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

    void Renderer::BeginFrame(const RenderView &view, float deltaTimeSeconds)
    {
        ResizeViewport(view.viewport.width, view.viewport.height);
        m_FrameData.Reset(view, ++m_FrameIndex, deltaTimeSeconds);
    }

    void Renderer::Render(const RenderView &view, float deltaTimeSeconds)
    {
        BeginFrame(view, deltaTimeSeconds);
        RenderClear();
        ProcessPendingCapture();
    }

    void Renderer::RenderScene(Scene &scene, const RenderView &view, float deltaTimeSeconds)
    {
        BeginFrame(view, deltaTimeSeconds);
        m_RenderProxy.Build(scene, view, m_FrameData);
        RenderClear();
        ProcessPendingCapture();
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

        BuildRenderGraph();
        m_RenderGraph.Execute(*commandList);
        m_Device->SubmitCommandList();
    }

    CaptureResult Renderer::CaptureCurrentView(const CaptureDesc &desc)
    {
        if (m_Device == nullptr || !HasValidRenderTarget())
        {
            return CaptureResult{false, desc.outputPath, "Renderer has no valid render target."};
        }

        RHI::RHICommandList *commandList = m_Device->GetCommandList();
        if (commandList == nullptr)
        {
            return CaptureResult{false, desc.outputPath, "Renderer command list is null."};
        }

        return RendererCapture::CaptureTexture(*commandList, *m_SceneColor, desc);
    }

    void Renderer::RequestCapture(const CaptureDesc &desc)
    {
        m_PendingCapture = desc;
    }

    bool Renderer::HasValidRenderTarget() const
    {
        return m_SceneColor != nullptr && m_Framebuffer != nullptr && m_ViewportWidth > 0 && m_ViewportHeight > 0;
    }

    void Renderer::ProcessPendingCapture()
    {
        if (!m_PendingCapture.has_value())
        {
            return;
        }

        CaptureDesc desc = *m_PendingCapture;
        m_PendingCapture.reset();

        CaptureResult result = CaptureCurrentView(desc);
        if (result.success)
        {
            PHYSARA_CORE_INFO("Renderer capture saved: {}", result.outputPath.string());
        }
        else
        {
            PHYSARA_CORE_ERROR("Renderer capture failed: {}", result.message);
        }
    }

    void Renderer::RecreateRenderTarget()
    {
        m_RenderGraph.Reset();
        m_Framebuffer.reset();
        m_SceneDepth.reset();
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

        RHI::RHITextureDesc sceneDepthDesc{};
        sceneDepthDesc.width = m_ViewportWidth;
        sceneDepthDesc.height = m_ViewportHeight;
        sceneDepthDesc.format = RHI::TextureFormat::Depth24Stencil8;
        sceneDepthDesc.dimension = RHI::TextureDimension::Tex2D;
        sceneDepthDesc.usage = RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled;
        sceneDepthDesc.mipLevels = 1;
        sceneDepthDesc.arrayLayers = 1;
        sceneDepthDesc.samples = 1;

        m_SceneDepth = m_Device->CreateTexture(sceneDepthDesc);
        if (m_SceneDepth == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneDepth render target.");
            m_SceneColor.reset();
            return;
        }

        RHI::RHIFramebufferDesc framebufferDesc{};
        framebufferDesc.colorAttachments.push_back(m_SceneColor.get());
        framebufferDesc.depthAttachment = m_SceneDepth.get();
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

    void Renderer::BuildRenderGraph()
    {
        m_RenderGraph.Reset();
        if (!HasValidRenderTarget())
        {
            return;
        }

        RenderGraphResourceHandle sceneColor = m_RenderGraph.ImportTexture("SceneColor", *m_SceneColor);
        m_RenderGraph.AddPass("ForwardOpaque")
            .Write(sceneColor)
            .SetExecute([this](RenderGraphContext &context)
                        {
                            ForwardPassContext passContext{};
                            passContext.device = m_Device;
                            passContext.commandList = &context.commandList;
                            passContext.framebuffer = m_Framebuffer.get();
                            passContext.renderPassDesc = &m_RenderPassDesc;
                            passContext.shaderLibrary = &m_ShaderLibrary;
                            passContext.pipelineCache = &m_PipelineStateCache;
                            passContext.frameData = &m_FrameData;
                            passContext.renderProxy = &m_RenderProxy;
                            passContext.clearColor = m_ClearColor;
                            m_ForwardOpaquePass.Execute(passContext);
                        });
    }
}