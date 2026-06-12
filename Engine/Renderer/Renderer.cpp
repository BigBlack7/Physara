#include "Renderer.hpp"

#include <algorithm>
#include <chrono>
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

        float ElapsedMilliseconds(std::chrono::steady_clock::time_point start)
        {
            const auto end = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration<float, std::milli>(end - start);
            return elapsed.count();
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
        m_RenderGraph.ReleasePooledResources();
        m_FrameUploadAllocator.Release();
        m_GPUScene.Release();
        m_Framebuffer.reset();
        m_FinalFramebuffer.reset();
        m_ShadowPass.Reset();
        m_ForwardOpaquePass.Reset();
        m_IBLResources.Reset();
        m_MeshGPUCache.Reset();
        m_SceneDepthMSAA.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColorMSAA.reset();
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
        m_FrameUploadAllocator.Reset();
        m_GPUScene.Reset();
        m_FrameData.Reset(view, ++m_FrameIndex, deltaTimeSeconds);
    }

    void Renderer::Render(const RenderView &view, float deltaTimeSeconds)
    {
        BeginFrame(view, deltaTimeSeconds);
        RenderClear();
        ProcessPendingCapture();
    }

    void Renderer::RenderScene(Scene &scene, const RenderView &view, float deltaTimeSeconds, bool transformsAlreadyUpdated)
    {
        BeginFrame(view, deltaTimeSeconds);
        const auto buildStart = std::chrono::steady_clock::now();
        if (!transformsAlreadyUpdated)
        {
            scene.UpdateTransforms();
        }
        m_RenderProxy.Build(scene, view, m_FrameData, m_AssetManager);
        m_FrameData.stats.sceneBuildCpuMs = RendererDetail::ElapsedMilliseconds(buildStart);
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

        const auto renderStart = std::chrono::steady_clock::now();
        commandList->ResetStatistics();
        BuildRenderGraph();
        m_RenderGraph.Execute(*commandList, m_Device);
        m_Device->SubmitCommandList();
        m_FrameData.stats.backend = commandList->GetStatistics();
        m_FrameData.stats.renderGraphCpuMs = RendererDetail::ElapsedMilliseconds(renderStart);
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
        m_IBLResources.Invalidate();
    }

    void Renderer::SetShadowSettings(const ShadowSettings &settings)
    {
        m_ShadowSettings = settings;
        m_ShadowPass.SetSettings(settings);
    }

    void Renderer::SetMSAASamples(std::uint32_t samples)
    {
        const std::uint32_t sanitized = samples >= 8u ? 8u : (samples >= 4u ? 4u : (samples >= 2u ? 2u : 1u));
        if (m_MSAASamples == sanitized)
        {
            return;
        }

        m_MSAASamples = sanitized;
        RecreateRenderTarget();
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
        m_RenderGraph.ReleasePooledResources();
        m_Framebuffer.reset();
        m_FinalFramebuffer.reset();
        m_SceneDepthMSAA.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColorMSAA.reset();
        m_SceneHDRColor.reset();

        if (m_Device == nullptr || m_ViewportWidth == 0 || m_ViewportHeight == 0)
        {
            return;
        }

        if (!m_RenderPassDesc.colorAttachments.empty())
        {
            m_RenderPassDesc.colorAttachments[0].samples = m_MSAASamples;
        }
        if (m_RenderPassDesc.hasDepth)
        {
            m_RenderPassDesc.depthAttachment.samples = m_MSAASamples;
        }
        m_SkyboxRenderPassDesc = m_RenderPassDesc;
        if (!m_SkyboxRenderPassDesc.colorAttachments.empty())
        {
            m_SkyboxRenderPassDesc.colorAttachments[0].loadOp = RHI::LoadOp::Load;
        }
        m_SkyboxRenderPassDesc.depthAttachment.loadOp = RHI::LoadOp::Load;

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

        RHI::RHITexture *frameColor = m_SceneHDRColor.get();
        RHI::RHITexture *frameDepth = m_SceneDepth.get();
        if (m_MSAASamples > 1u)
        {
            RHI::RHITextureDesc sceneHDRMSAADesc = sceneHDRDesc;
            sceneHDRMSAADesc.samples = m_MSAASamples;
            sceneHDRMSAADesc.usage = RHI::TextureUsage::RenderTarget;
            m_SceneHDRColorMSAA = m_Device->CreateTexture(sceneHDRMSAADesc);

            RHI::RHITextureDesc sceneDepthMSAADesc = sceneDepthDesc;
            sceneDepthMSAADesc.samples = m_MSAASamples;
            sceneDepthMSAADesc.usage = RHI::TextureUsage::DepthStencil;
            m_SceneDepthMSAA = m_Device->CreateTexture(sceneDepthMSAADesc);

            if (m_SceneHDRColorMSAA == nullptr || m_SceneDepthMSAA == nullptr)
            {
                PHYSARA_CORE_ERROR("Renderer failed to create MSAA render targets.");
                m_SceneHDRColorMSAA.reset();
                m_SceneDepthMSAA.reset();
                m_SceneHDRColor.reset();
                m_SceneColor.reset();
                m_SceneDepth.reset();
                return;
            }

            frameColor = m_SceneHDRColorMSAA.get();
            frameDepth = m_SceneDepthMSAA.get();
        }

        RHI::RHIFramebufferDesc framebufferDesc{};
        framebufferDesc.colorAttachments.push_back(frameColor);
        framebufferDesc.depthAttachment = frameDepth;
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
        RenderGraphResourceHandle sceneDepth = m_RenderGraph.ImportTexture("SceneDepth", *m_SceneDepth);
        RenderGraphResourceHandle sceneColor = m_RenderGraph.ImportTexture("SceneColor", *m_SceneColor);
        const bool msaaEnabled = m_MSAASamples > 1u && m_SceneHDRColorMSAA != nullptr && m_SceneDepthMSAA != nullptr;
        RenderGraphResourceHandle renderHDR = msaaEnabled ? m_RenderGraph.ImportTexture("SceneHDRMSAA", *m_SceneHDRColorMSAA) : sceneHDR;
        RenderGraphResourceHandle renderDepth = msaaEnabled ? m_RenderGraph.ImportTexture("SceneDepthMSAA", *m_SceneDepthMSAA) : sceneDepth;
        m_RenderGraph.MarkOutput(sceneColor);
        const bool drawSkybox = m_SkyboxEnabled && !m_EnvironmentMapPath.empty();
        if (!m_EnvironmentMapPath.empty())
        {
            (void)m_IBLResources.Ensure(m_Device, m_EnvironmentMapPath);
        }

        m_RenderGraph.AddPass("GPUSceneUpload")
            .SetSideEffect()
            .SetExecute([this](RenderGraphContext &)
                        {
                            if (m_Device == nullptr)
                            {
                                return;
                            }

                            m_GPUScene.UploadFrame(
                                *m_Device,
                                m_FrameUploadAllocator,
                                m_FrameData,
                                m_RenderProxy,
                                m_AssetManager,
                                &m_FrameData.stats);
                        });

        if (m_ShadowSettings.algorithm != ShadowAlgorithm::None)
        {
            m_RenderGraph.AddPass("Shadow")
                .SetSideEffect()
                .SetExecute([this](RenderGraphContext &context)
                            {
                                ShadowPassContext passContext{};
                                passContext.device = m_Device;
                                passContext.commandList = &context.commandList;
                                passContext.shaderLibrary = &m_ShaderLibrary;
                                passContext.pipelineCache = &m_PipelineStateCache;
                                passContext.frameData = &m_FrameData;
                                passContext.frameUploadAllocator = &m_FrameUploadAllocator;
                                passContext.gpuScene = &m_GPUScene;
                                passContext.stats = &m_FrameData.stats;
                                passContext.renderProxy = &m_RenderProxy;
                                passContext.meshCache = &m_MeshGPUCache;
                                passContext.assetManager = m_AssetManager;
                                const auto passStart = std::chrono::steady_clock::now();
                                m_ShadowPass.Execute(passContext);
                                m_FrameData.stats.shadowCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                            });
        }

        if (drawSkybox)
        {
            m_RenderGraph.AddPass("Skybox")
                .Write(renderHDR)
                .Write(renderDepth)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                SkyboxPassContext passContext{};
                                passContext.device = m_Device;
                                passContext.commandList = &context.commandList;
                                passContext.framebuffer = m_Framebuffer.get();
                                passContext.renderPassDesc = &m_RenderPassDesc;
                                passContext.shaderLibrary = &m_ShaderLibrary;
                                passContext.pipelineCache = &m_PipelineStateCache;
                                passContext.frameData = &m_FrameData;
                                passContext.frameUploadAllocator = &m_FrameUploadAllocator;
                                passContext.stats = &m_FrameData.stats;
                                passContext.environmentPath = m_EnvironmentMapPath;
                                passContext.exposureCompensation = m_SkyboxExposureCompensation;
                                passContext.enabled = true;
                                const auto passStart = std::chrono::steady_clock::now();
                                m_SkyboxPass.Execute(passContext);
                                m_FrameData.stats.skyboxCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                            });
        }

        m_RenderGraph.AddPass("ForwardOpaque")
            .Read(renderHDR)
            .Read(renderDepth)
            .Write(renderHDR)
            .Write(renderDepth)
            .SetExecute([this](RenderGraphContext &context)
                        {
                            if (m_ShadowPass.GetShadowMap() != nullptr && m_FrameData.shadow.params.x > 0.5f)
                            {
                                RHI::RHIResourceBarrier shadowBarrier{};
                                shadowBarrier.before = RHI::ResourceState::DepthWrite;
                                shadowBarrier.after = RHI::ResourceState::ShaderResource;
                                shadowBarrier.srcStages = RHI::ShaderStageBit::Fragment;
                                shadowBarrier.dstStages = RHI::ShaderStageBit::Fragment;
                                shadowBarrier.srcAccess = RHI::ResourceAccess::DepthStencilWrite;
                                shadowBarrier.dstAccess = RHI::ResourceAccess::ShaderRead;
                                context.commandList.TextureBarrier(m_ShadowPass.GetShadowMap(), shadowBarrier);
                            }

                            ForwardPassContext passContext{};
                            passContext.device = m_Device;
                            passContext.commandList = &context.commandList;
                            passContext.framebuffer = m_Framebuffer.get();
                            passContext.renderPassDesc = m_SkyboxEnabled && !m_EnvironmentMapPath.empty() ? &m_SkyboxRenderPassDesc : &m_RenderPassDesc;
                            passContext.shaderLibrary = &m_ShaderLibrary;
                            passContext.pipelineCache = &m_PipelineStateCache;
                            passContext.frameData = &m_FrameData;
                            passContext.frameUploadAllocator = &m_FrameUploadAllocator;
                            passContext.gpuScene = &m_GPUScene;
                            passContext.stats = &m_FrameData.stats;
                            passContext.renderProxy = &m_RenderProxy;
                            passContext.meshCache = &m_MeshGPUCache;
                            passContext.assetManager = m_AssetManager;
                            passContext.shadowMap = m_ShadowPass.GetShadowMap();
                            passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
                            passContext.environmentExposureCompensation = m_SkyboxExposureCompensation;
                            passContext.clearColor = RendererDetail::BuildSceneReferredClearColor(m_ClearColor, m_FrameData.view.ev100);
                            passContext.debugView = static_cast<std::uint32_t>(m_PostProcessSettings.debugView);
                            const auto passStart = std::chrono::steady_clock::now();
                            m_ForwardOpaquePass.Execute(passContext);
                            m_FrameData.stats.forwardOpaqueCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                        });

        if (!m_RenderProxy.GetBuckets().transparent.empty())
        {
            m_RenderGraph.AddPass("ForwardTransparent")
                .Read(renderHDR)
                .Write(renderHDR)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                ExecuteTransparentForwardPass(context);
                            });
        }

        if (msaaEnabled)
        {
            m_RenderGraph.AddPass("MSAAResolve")
                .Read(renderHDR)
                .Read(renderDepth)
                .Write(sceneHDR)
                .Write(sceneDepth)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                context.commandList.ResolveTexture(m_SceneHDRColorMSAA.get(), m_SceneHDRColor.get());
                                context.commandList.ResolveTexture(m_SceneDepthMSAA.get(), m_SceneDepth.get());
                            });
        }

        m_RenderGraph.AddPass("PostProcess")
            .Read(sceneHDR)
            .Read(sceneDepth)
            .Write(sceneColor)
            .SetExecute([this](RenderGraphContext &context)
                        {
                            PostProcessPassContext passContext{};
                            passContext.device = m_Device;
                            passContext.commandList = &context.commandList;
                            passContext.framebuffer = m_FinalFramebuffer.get();
                            passContext.renderPassDesc = &m_FinalRenderPassDesc;
                            passContext.shaderLibrary = &m_ShaderLibrary;
                            passContext.pipelineCache = &m_PipelineStateCache;
                            passContext.frameData = &m_FrameData;
                            passContext.frameUploadAllocator = &m_FrameUploadAllocator;
                            passContext.stats = &m_FrameData.stats;
                            passContext.sceneHDR = m_SceneHDRColor.get();
                            passContext.sceneDepth = m_SceneDepth.get();
                            passContext.settings = m_PostProcessSettings;
                            const auto passStart = std::chrono::steady_clock::now();
                            m_PostProcessPass.Execute(passContext);
                            m_FrameData.stats.postProcessCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                        });
    }

    void Renderer::ExecuteTransparentForwardPass(RenderGraphContext &context)
    {
        ForwardPassContext passContext{};
        passContext.device = m_Device;
        passContext.commandList = &context.commandList;
        passContext.framebuffer = m_Framebuffer.get();
        passContext.renderPassDesc = &m_SkyboxRenderPassDesc;
        passContext.shaderLibrary = &m_ShaderLibrary;
        passContext.pipelineCache = &m_PipelineStateCache;
        passContext.frameData = &m_FrameData;
        passContext.frameUploadAllocator = &m_FrameUploadAllocator;
        passContext.gpuScene = &m_GPUScene;
        passContext.stats = &m_FrameData.stats;
        passContext.renderProxy = &m_RenderProxy;
        passContext.meshCache = &m_MeshGPUCache;
        passContext.assetManager = m_AssetManager;
        passContext.shadowMap = m_ShadowPass.GetShadowMap();
        passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
        passContext.environmentExposureCompensation = m_SkyboxExposureCompensation;
        passContext.debugView = static_cast<std::uint32_t>(m_PostProcessSettings.debugView);
        const auto passStart = std::chrono::steady_clock::now();
        m_ForwardOpaquePass.ExecuteTransparent(passContext);
        m_FrameData.stats.forwardTransparentCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
    }
}