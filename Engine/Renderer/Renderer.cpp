#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    namespace RendererDetail
    {
        float ExposureFromEV100(float ev100)
        {
            return 1.f / (std::pow(2.f, ev100) * 1.2f);
        }

        glm::vec4 BuildSceneReferredClearColor(const glm::vec4 &displayColor, float ev100)
        {
            const float inverseExposure = 1.f / std::max(ExposureFromEV100(ev100), 0.000001f);
            return glm::vec4(glm::vec3(displayColor) * inverseExposure, displayColor.a);
        }
    }

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
            RHI::TextureFormat::RGBA16F,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_RenderPassDesc.depthAttachment = {
            RHI::TextureFormat::Depth24Stencil8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u};
        m_RenderPassDesc.hasDepth = true;
        m_SkyboxRenderPassDesc = m_RenderPassDesc;
        if (!m_SkyboxRenderPassDesc.colorAttachments.empty())
        {
            m_SkyboxRenderPassDesc.colorAttachments[0].loadOp = RHI::LoadOp::Load;
        }
        m_SkyboxRenderPassDesc.depthAttachment.loadOp = RHI::LoadOp::Load;
        m_FinalRenderPassDesc = {};
        m_FinalRenderPassDesc.colorAttachments.push_back({
            RHI::TextureFormat::RGBA8,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_FinalRenderPassDesc.hasDepth = false;

        if (m_Device == nullptr)
        {
            PHYSARA_CORE_WARN("Renderer initialized without RHIDevice.");
        }
    }

    void Renderer::Shutdown()
    {
        m_RenderGraph.Reset();
        m_Framebuffer.reset();
        m_FinalFramebuffer.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColor.reset();
        m_ShaderLibrary.SetDevice(nullptr);
        m_PipelineStateCache.SetDevice(nullptr);
        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
        m_Device = nullptr;
        m_AssetManager = nullptr;
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

    void Renderer::SetEnvironmentMapPath(std::filesystem::path path)
    {
        path = path.lexically_normal();
        if (m_EnvironmentMapPath == path)
        {
            return;
        }

        m_EnvironmentMapPath = std::move(path);
        m_SkyboxPass.InvalidateEnvironment();
    }

    bool Renderer::HasValidRenderTarget() const
    {
        return m_SceneHDRColor != nullptr && m_SceneColor != nullptr && m_Framebuffer != nullptr &&
               m_FinalFramebuffer != nullptr && m_ViewportWidth > 0 && m_ViewportHeight > 0;
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
        m_FinalFramebuffer.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColor.reset();

        if (m_Device == nullptr || m_ViewportWidth == 0 || m_ViewportHeight == 0)
        {
            return;
        }

        RHI::RHITextureDesc sceneHDRDesc{};
        sceneHDRDesc.width = m_ViewportWidth;
        sceneHDRDesc.height = m_ViewportHeight;
        sceneHDRDesc.format = RHI::TextureFormat::RGBA16F;
        sceneHDRDesc.dimension = RHI::TextureDimension::Tex2D;
        sceneHDRDesc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::RenderTarget;
        sceneHDRDesc.mipLevels = 1;
        sceneHDRDesc.arrayLayers = 1;
        sceneHDRDesc.samples = 1;

        m_SceneHDRColor = m_Device->CreateTexture(sceneHDRDesc);
        if (m_SceneHDRColor == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneHDR render target.");
            return;
        }

        RHI::RHITextureDesc sceneColorDesc = sceneHDRDesc;
        sceneColorDesc.format = RHI::TextureFormat::RGBA8;
        m_SceneColor = m_Device->CreateTexture(sceneColorDesc);
        if (m_SceneColor == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneColor render target.");
            m_SceneHDRColor.reset();
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
            m_SceneHDRColor.reset();
            m_SceneColor.reset();
            return;
        }

        RHI::RHIFramebufferDesc framebufferDesc{};
        framebufferDesc.colorAttachments.push_back(m_SceneHDRColor.get());
        framebufferDesc.depthAttachment = m_SceneDepth.get();
        framebufferDesc.width = m_ViewportWidth;
        framebufferDesc.height = m_ViewportHeight;
        framebufferDesc.renderPassDesc = &m_RenderPassDesc;

        m_Framebuffer = m_Device->CreateFramebuffer(framebufferDesc);
        if (m_Framebuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneHDR framebuffer.");
            m_SceneHDRColor.reset();
            m_SceneColor.reset();
            m_SceneDepth.reset();
            return;
        }

        RHI::RHIFramebufferDesc finalFramebufferDesc{};
        finalFramebufferDesc.colorAttachments.push_back(m_SceneColor.get());
        finalFramebufferDesc.width = m_ViewportWidth;
        finalFramebufferDesc.height = m_ViewportHeight;
        finalFramebufferDesc.renderPassDesc = &m_FinalRenderPassDesc;

        m_FinalFramebuffer = m_Device->CreateFramebuffer(finalFramebufferDesc);
        if (m_FinalFramebuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Renderer failed to create SceneColor framebuffer.");
            m_Framebuffer.reset();
            m_SceneHDRColor.reset();
            m_SceneColor.reset();
            m_SceneDepth.reset();
        }
    }

    void Renderer::BuildRenderGraph()
    {
        m_RenderGraph.Reset();
        if (!HasValidRenderTarget())
        {
            return;
        }

        RenderGraphResourceHandle sceneHDR = m_RenderGraph.ImportTexture("SceneHDR", *m_SceneHDRColor);
        RenderGraphResourceHandle sceneColor = m_RenderGraph.ImportTexture("SceneColor", *m_SceneColor);
        const bool drawSkybox = m_SkyboxEnabled && !m_EnvironmentMapPath.empty();

        m_RenderGraph.AddPass("ForwardOpaque")
            .Write(sceneHDR)
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
                            passContext.assetManager = m_AssetManager;
                            passContext.clearColor = RendererDetail::BuildSceneReferredClearColor(m_ClearColor, m_FrameData.view.ev100);
                            m_ForwardOpaquePass.Execute(passContext);
                        });

        if (drawSkybox)
        {
            m_RenderGraph.AddPass("Skybox")
                .Read(sceneHDR)
                .Write(sceneHDR)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                SkyboxPassContext passContext{};
                                passContext.device = m_Device;
                                passContext.commandList = &context.commandList;
                                passContext.framebuffer = m_Framebuffer.get();
                                passContext.renderPassDesc = &m_SkyboxRenderPassDesc;
                                passContext.shaderLibrary = &m_ShaderLibrary;
                                passContext.pipelineCache = &m_PipelineStateCache;
                                passContext.frameData = &m_FrameData;
                                passContext.environmentPath = m_EnvironmentMapPath;
                                passContext.exposureCompensation = m_SkyboxExposureCompensation;
                                passContext.enabled = true;
                                m_SkyboxPass.Execute(passContext);
                            });
        }

        m_RenderGraph.AddPass("PostProcess")
            .Read(sceneHDR)
            .Write(sceneColor)
            .SetExecute([this](RenderGraphContext &context)
                        {
                            RHI::RHIResourceBarrier barrier{};
                            barrier.before = RHI::ResourceState::RenderTarget;
                            barrier.after = RHI::ResourceState::ShaderResource;
                            barrier.srcStages = RHI::ShaderStageBit::Fragment;
                            barrier.dstStages = RHI::ShaderStageBit::Fragment;
                            barrier.srcAccess = RHI::ResourceAccess::ColorAttachmentWrite;
                            barrier.dstAccess = RHI::ResourceAccess::ShaderRead;
                            context.commandList.TextureBarrier(m_SceneHDRColor.get(), barrier);

                            PostProcessPassContext passContext{};
                            passContext.device = m_Device;
                            passContext.commandList = &context.commandList;
                            passContext.framebuffer = m_FinalFramebuffer.get();
                            passContext.renderPassDesc = &m_FinalRenderPassDesc;
                            passContext.shaderLibrary = &m_ShaderLibrary;
                            passContext.pipelineCache = &m_PipelineStateCache;
                            passContext.frameData = &m_FrameData;
                            passContext.sceneHDR = m_SceneHDRColor.get();
                            passContext.settings = m_PostProcessSettings;
                            m_PostProcessPass.Execute(passContext);
                        });
    }
}