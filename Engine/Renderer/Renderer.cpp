#include "Renderer.hpp"

#include <algorithm>
#include <chrono>
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
        glm::vec4 BuildPreExposedClearColor(const glm::vec4 &displayColor)
        {
            return displayColor;
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
        m_GBufferPass.Reset();
        m_DeferredLightingPass.Reset();
        m_DeferredResources.Reset();
        m_MaterialTextureCache.Reset();
        m_IBLResources.Reset();
        m_MeshGPUCache.Reset();
        m_SceneDepthMSAA.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColorMSAA.reset();
        m_SceneHDRColor.reset();
        InvalidateCommandState();
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
        if (UsesClusteredLighting(m_RenderPath))
        {
            m_ClusteredLightGrid.Build(m_FrameData);
        }
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
        InvalidateCommandState();
    }

    void Renderer::SetShadowSettings(const ShadowSettings &settings)
    {
        const std::uint32_t previousResolution = m_ShadowPass.GetSettings().resolution;
        const std::uint32_t previousCascadeCount = m_ShadowPass.GetSettings().cascadeCount;
        m_ShadowPass.SetSettings(settings);
        m_ShadowSettings = m_ShadowPass.GetSettings();
        if (previousResolution != m_ShadowSettings.resolution ||
            previousCascadeCount != m_ShadowSettings.cascadeCount)
        {
            InvalidateCommandState();
        }
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

    void Renderer::SetPostProcessSettings(const PostProcessSettings &settings)
    {
        m_PostProcessSettings = settings;
        const bool gBufferView = m_PostProcessSettings.debugView >= DebugViewMode::GBufferBaseColor;
        const bool clusterView = m_PostProcessSettings.debugView == DebugViewMode::LightClusters;
        if ((gBufferView && m_RenderPath != RenderPath::Deferred) ||
            (clusterView && !UsesClusteredLighting(m_RenderPath)))
        {
            m_PostProcessSettings.debugView = DebugViewMode::None;
        }
    }

    void Renderer::SetRenderPath(RenderPath path)
    {
        if (m_RenderPath == path)
        {
            return;
        }

        m_RenderPath = path;
        SetPostProcessSettings(m_PostProcessSettings);
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

    void Renderer::InvalidateCommandState()
    {
        if (m_Device == nullptr)
        {
            return;
        }

        RHI::RHICommandList *commandList = m_Device->GetCommandList();
        if (commandList != nullptr)
        {
            commandList->InvalidateExternalState();
        }
    }

    void Renderer::RecreateRenderTarget()
    {
        m_RenderGraph.Reset();
        m_RenderGraph.ReleasePooledResources();
        m_Framebuffer.reset();
        m_FinalFramebuffer.reset();
        m_DeferredResources.Reset();
        m_SceneDepthMSAA.reset();
        m_SceneDepth.reset();
        m_SceneColor.reset();
        m_SceneHDRColorMSAA.reset();
        m_SceneHDRColor.reset();
        InvalidateCommandState();

        if (m_Device == nullptr || m_ViewportWidth == 0 || m_ViewportHeight == 0)
        {
            return;
        }

        const std::uint32_t effectiveSamples = m_RenderPath == RenderPath::Deferred ? 1u : m_MSAASamples;
        if (!m_RenderPassDesc.colorAttachments.empty())
        {
            m_RenderPassDesc.colorAttachments[0].samples = effectiveSamples;
        }
        if (m_RenderPassDesc.hasDepth)
        {
            m_RenderPassDesc.depthAttachment.samples = effectiveSamples;
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
        if (effectiveSamples > 1u)
        {
            RHI::RHITextureDesc sceneHDRMSAADesc = sceneHDRDesc;
            sceneHDRMSAADesc.samples = effectiveSamples;
            sceneHDRMSAADesc.usage = RHI::TextureUsage::RenderTarget;
            m_SceneHDRColorMSAA = m_Device->CreateTexture(sceneHDRMSAADesc);

            RHI::RHITextureDesc sceneDepthMSAADesc = sceneDepthDesc;
            sceneDepthMSAADesc.samples = effectiveSamples;
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
        const bool deferred = m_RenderPath == RenderPath::Deferred;
        if (deferred && !m_DeferredResources.Ensure(
                            *m_Device,
                            m_ViewportWidth,
                            m_ViewportHeight,
                            *m_SceneHDRColor,
                            *m_SceneDepth))
        {
            PHYSARA_CORE_ERROR("Renderer failed to prepare deferred render targets.");
            return;
        }
        const bool msaaEnabled = !deferred && m_MSAASamples > 1u &&
                                 m_SceneHDRColorMSAA != nullptr && m_SceneDepthMSAA != nullptr;
        RenderGraphResourceHandle renderHDR = msaaEnabled ? m_RenderGraph.ImportTexture("SceneHDRMSAA", *m_SceneHDRColorMSAA) : sceneHDR;
        RenderGraphResourceHandle renderDepth = msaaEnabled ? m_RenderGraph.ImportTexture("SceneDepthMSAA", *m_SceneDepthMSAA) : sceneDepth;
        RenderGraphResourceHandle gBufferBaseColor{};
        RenderGraphResourceHandle gBufferNormal{};
        RenderGraphResourceHandle gBufferMaterial{};
        RenderGraphResourceHandle gBufferEmissive{};
        if (deferred)
        {
            gBufferBaseColor = m_RenderGraph.ImportTexture("GBufferBaseColor", *m_DeferredResources.GetBaseColor());
            gBufferNormal = m_RenderGraph.ImportTexture("GBufferNormal", *m_DeferredResources.GetNormal());
            gBufferMaterial = m_RenderGraph.ImportTexture("GBufferMaterial", *m_DeferredResources.GetMaterial());
            gBufferEmissive = m_RenderGraph.ImportTexture("GBufferEmissive", *m_DeferredResources.GetEmissive());
        }
        RenderGraphResourceHandle shadowMap{};
        const bool shadowEnabled = m_ShadowSettings.enabled;
        if (shadowEnabled)
        {
            m_ShadowPass.PrepareResources(*m_Device);
            if (RHI::RHITexture *shadowTexture = m_ShadowPass.GetShadowMap())
            {
                shadowMap = m_RenderGraph.ImportTexture("ShadowMap", *shadowTexture);
            }
        }
        m_RenderGraph.MarkOutput(sceneColor);
        const bool gBufferDebug = deferred && m_PostProcessSettings.debugView >= DebugViewMode::GBufferBaseColor;
        const bool drawSkybox = m_SkyboxEnabled && !m_EnvironmentMapPath.empty() && !gBufferDebug;
        if (!m_EnvironmentMapPath.empty())
        {
            (void)m_IBLResources.Ensure(m_Device, m_EnvironmentMapPath);
        }

        m_RenderGraph.AddPass("GPUSceneUpload")
            .SetSideEffect()
            .SetExecute([this](RenderGraphContext &context)
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
                            m_MaterialTextureCache.Update(
                                *m_Device,
                                context.commandList,
                                m_AssetManager,
                                m_FrameData,
                                &m_FrameData.stats);
                        });

        if (shadowEnabled)
        {
            RGBuilder shadowPass = m_RenderGraph.AddPass("Shadow").SetSideEffect();
            if (shadowMap.IsValid())
            {
                shadowPass.WriteAttachment(shadowMap);
            }
            shadowPass.SetExecute([this](RenderGraphContext &context)
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

        m_RenderGraph.AddPass("FrameUniformsUpload")
            .SetSideEffect()
            .SetExecute([this](RenderGraphContext &)
                        {
                            if (m_Device == nullptr)
                            {
                                return;
                            }

                            m_GPUScene.UploadFrameUniforms(
                                *m_Device,
                                m_FrameUploadAllocator,
                                m_FrameData,
                                m_IBLResources.IsReady() ? &m_IBLResources : nullptr,
                                m_SkyboxExposureCompensation,
                                static_cast<std::uint32_t>(m_PostProcessSettings.debugView),
                                &m_FrameData.stats);
                            m_FrameUploadAllocator.Flush(&m_FrameData.stats);
                        });

        if (drawSkybox)
        {
            m_RenderGraph.AddPass("Skybox")
                .WriteAttachment(renderHDR)
                .WriteAttachment(renderDepth)
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
                                passContext.gpuScene = &m_GPUScene;
                                passContext.stats = &m_FrameData.stats;
                                passContext.environmentPath = m_EnvironmentMapPath;
                                passContext.exposureCompensation = m_SkyboxExposureCompensation;
                                passContext.enabled = true;
                                const auto passStart = std::chrono::steady_clock::now();
                                m_SkyboxPass.Execute(passContext);
                                m_FrameData.stats.skyboxCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                            });
        }

        if (deferred)
        {
            m_RenderGraph.AddPass("GBuffer")
                .WriteAttachment(gBufferBaseColor)
                .WriteAttachment(gBufferNormal)
                .WriteAttachment(gBufferMaterial)
                .WriteAttachment(gBufferEmissive)
                .WriteAttachment(sceneDepth)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                GBufferPassContext passContext{};
                                passContext.device = m_Device;
                                passContext.commandList = &context.commandList;
                                passContext.framebuffer = m_DeferredResources.GetGBufferFramebuffer();
                                passContext.renderPassDesc = &m_DeferredResources.GetGBufferRenderPassDesc();
                                passContext.shaderLibrary = &m_ShaderLibrary;
                                passContext.pipelineCache = &m_PipelineStateCache;
                                passContext.frameData = &m_FrameData;
                                passContext.gpuScene = &m_GPUScene;
                                passContext.stats = &m_FrameData.stats;
                                passContext.renderProxy = &m_RenderProxy;
                                passContext.meshCache = &m_MeshGPUCache;
                                passContext.assetManager = m_AssetManager;
                                passContext.materialTextureCache = &m_MaterialTextureCache;
                                passContext.wireframe = m_PostProcessSettings.debugView == DebugViewMode::Wireframe;
                                const auto passStart = std::chrono::steady_clock::now();
                                m_GBufferPass.Execute(passContext);
                                m_FrameData.stats.deferredGBufferCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                            });

            RGBuilder deferredLighting = m_RenderGraph.AddPass("DeferredLighting")
                .ReadTexture(gBufferBaseColor)
                .ReadTexture(gBufferNormal)
                .ReadTexture(gBufferMaterial)
                .ReadTexture(gBufferEmissive)
                .ReadTexture(sceneDepth)
                .WriteAttachment(sceneHDR);
            if (drawSkybox)
            {
                deferredLighting.Read(
                    sceneHDR,
                    RHI::ResourceState::RenderTarget,
                    RHI::ShaderStageBit::Fragment,
                    RHI::ResourceAccess::ColorAttachmentRead);
            }
            if (shadowMap.IsValid())
            {
                deferredLighting.ReadTexture(shadowMap);
            }
            deferredLighting.SetExecute([this, drawSkybox, gBufferDebug](RenderGraphContext &context)
                                        {
                                            DeferredLightingPassContext passContext{};
                                            passContext.device = m_Device;
                                            passContext.commandList = &context.commandList;
                                            passContext.framebuffer = m_DeferredResources.GetLightingFramebuffer();
                                            passContext.renderPassDesc = drawSkybox
                                                                             ? &m_DeferredResources.GetLightingLoadRenderPassDesc()
                                                                             : &m_DeferredResources.GetLightingClearRenderPassDesc();
                                            passContext.shaderLibrary = &m_ShaderLibrary;
                                            passContext.pipelineCache = &m_PipelineStateCache;
                                            passContext.frameData = &m_FrameData;
                                            passContext.gpuScene = &m_GPUScene;
                                            passContext.stats = &m_FrameData.stats;
                                            passContext.sceneDepth = m_SceneDepth.get();
                                            passContext.baseColor = m_DeferredResources.GetBaseColor();
                                            passContext.normal = m_DeferredResources.GetNormal();
                                            passContext.material = m_DeferredResources.GetMaterial();
                                            passContext.emissive = m_DeferredResources.GetEmissive();
                                            passContext.shadowMap = m_ShadowPass.GetShadowMap();
                                            passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
                                            passContext.clearColor = gBufferDebug
                                                                         ? glm::vec4(0.f, 0.f, 0.f, 1.f)
                                                                         : RendererDetail::BuildPreExposedClearColor(m_ClearColor);
                                            const auto passStart = std::chrono::steady_clock::now();
                                            m_DeferredLightingPass.Execute(passContext);
                                            m_FrameData.stats.deferredLightingCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                                        });

            if (!gBufferDebug && !m_RenderProxy.GetBuckets().unlit.Empty())
            {
                m_RenderGraph.AddPass("ForwardUnlit")
                    .Read(
                        sceneHDR,
                        RHI::ResourceState::RenderTarget,
                        RHI::ShaderStageBit::Fragment,
                        RHI::ResourceAccess::ColorAttachmentRead)
                    .WriteAttachment(sceneHDR)
                    .WriteAttachment(sceneDepth)
                    .SetExecute([this](RenderGraphContext &context)
                                {
                                    ExecuteUnlitForwardPass(context);
                                });
            }
        }
        else
        {
            RGBuilder forwardOpaque = m_RenderGraph.AddPass(
                                                   m_RenderPath == RenderPath::ForwardPlus
                                                       ? "ForwardPlusOpaque"
                                                       : "ForwardOpaque")
                                                  .WriteAttachment(renderHDR)
                                                  .WriteAttachment(renderDepth);
            if (shadowMap.IsValid())
            {
                forwardOpaque.ReadTexture(shadowMap);
            }
            if (drawSkybox)
            {
                forwardOpaque.Read(
                    renderHDR,
                    RHI::ResourceState::RenderTarget,
                    RHI::ShaderStageBit::Fragment,
                    RHI::ResourceAccess::ColorAttachmentRead);
            }
            forwardOpaque.SetExecute([this](RenderGraphContext &context)
                                     {
                                         ForwardPassContext passContext{};
                                         passContext.device = m_Device;
                                         passContext.commandList = &context.commandList;
                                         passContext.framebuffer = m_Framebuffer.get();
                                         passContext.renderPassDesc = m_SkyboxEnabled && !m_EnvironmentMapPath.empty()
                                                                          ? &m_SkyboxRenderPassDesc
                                                                          : &m_RenderPassDesc;
                                         passContext.shaderLibrary = &m_ShaderLibrary;
                                         passContext.pipelineCache = &m_PipelineStateCache;
                                         passContext.frameData = &m_FrameData;
                                         passContext.gpuScene = &m_GPUScene;
                                         passContext.stats = &m_FrameData.stats;
                                         passContext.renderProxy = &m_RenderProxy;
                                         passContext.meshCache = &m_MeshGPUCache;
                                         passContext.assetManager = m_AssetManager;
                                         passContext.materialTextureCache = &m_MaterialTextureCache;
                                         passContext.shadowMap = m_ShadowPass.GetShadowMap();
                                         passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
                                         passContext.environmentExposureCompensation = m_SkyboxExposureCompensation;
                                         passContext.clearColor = RendererDetail::BuildPreExposedClearColor(m_ClearColor);
                                         passContext.debugView = static_cast<std::uint32_t>(m_PostProcessSettings.debugView);
                                         passContext.lightingMode = UsesClusteredLighting(m_RenderPath)
                                                                        ? ForwardLightingMode::Clustered
                                                                        : ForwardLightingMode::FullLightList;
                                         passContext.wireframe = m_PostProcessSettings.debugView == DebugViewMode::Wireframe;
                                         const auto passStart = std::chrono::steady_clock::now();
                                         m_ForwardOpaquePass.Execute(passContext);
                                         m_FrameData.stats.forwardOpaqueCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
                                     });
        }

        if (!gBufferDebug && !m_RenderProxy.GetBuckets().transparent.Empty())
        {
            RGBuilder forwardTransparent = m_RenderGraph.AddPass("ForwardTransparent")
                .WriteAttachment(renderHDR);
            forwardTransparent.Read(
                renderHDR,
                RHI::ResourceState::RenderTarget,
                RHI::ShaderStageBit::Fragment,
                RHI::ResourceAccess::ColorAttachmentRead);
            forwardTransparent.Read(
                deferred ? sceneDepth : renderDepth,
                RHI::ResourceState::DepthRead,
                RHI::ShaderStageBit::Fragment,
                RHI::ResourceAccess::DepthStencilRead);
            if (shadowMap.IsValid())
            {
                forwardTransparent.ReadTexture(shadowMap);
            }
            forwardTransparent.SetExecute([this](RenderGraphContext &context)
                                          {
                                              ExecuteTransparentForwardPass(context);
                                          });
        }

        if (msaaEnabled)
        {
            m_RenderGraph.AddPass("MSAAResolve")
                .ReadTransfer(renderHDR)
                .ReadTransfer(renderDepth)
                .WriteTransfer(sceneHDR)
                .WriteTransfer(sceneDepth)
                .SetExecute([this](RenderGraphContext &context)
                            {
                                context.commandList.ResolveTexture(m_SceneHDRColorMSAA.get(), m_SceneHDRColor.get());
                                context.commandList.ResolveTexture(m_SceneDepthMSAA.get(), m_SceneDepth.get());
                            });
        }

        RGBuilder postProcess = m_RenderGraph.AddPass("PostProcess")
            .ReadTexture(sceneHDR)
            .ReadTexture(sceneDepth)
            .WriteAttachment(sceneColor);
        if (shadowMap.IsValid())
        {
            postProcess.ReadTexture(shadowMap);
        }
        postProcess.SetExecute([this](RenderGraphContext &context)
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
                            passContext.gpuScene = &m_GPUScene;
                            passContext.stats = &m_FrameData.stats;
                            passContext.sceneHDR = m_SceneHDRColor.get();
                            passContext.sceneDepth = m_SceneDepth.get();
                            passContext.shadowMap = m_ShadowPass.GetShadowMap();
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
        passContext.gpuScene = &m_GPUScene;
        passContext.stats = &m_FrameData.stats;
        passContext.renderProxy = &m_RenderProxy;
        passContext.meshCache = &m_MeshGPUCache;
        passContext.assetManager = m_AssetManager;
        passContext.materialTextureCache = &m_MaterialTextureCache;
        passContext.shadowMap = m_ShadowPass.GetShadowMap();
        passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
        passContext.environmentExposureCompensation = m_SkyboxExposureCompensation;
        passContext.debugView = static_cast<std::uint32_t>(m_PostProcessSettings.debugView);
        passContext.lightingMode = UsesClusteredLighting(m_RenderPath)
                                       ? ForwardLightingMode::Clustered
                                       : ForwardLightingMode::FullLightList;
        passContext.wireframe = m_PostProcessSettings.debugView == DebugViewMode::Wireframe;
        const auto passStart = std::chrono::steady_clock::now();
        m_ForwardOpaquePass.ExecuteTransparent(passContext);
        m_FrameData.stats.forwardTransparentCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
    }

    void Renderer::ExecuteUnlitForwardPass(RenderGraphContext &context)
    {
        ForwardPassContext passContext{};
        passContext.device = m_Device;
        passContext.commandList = &context.commandList;
        passContext.framebuffer = m_Framebuffer.get();
        passContext.renderPassDesc = &m_SkyboxRenderPassDesc;
        passContext.shaderLibrary = &m_ShaderLibrary;
        passContext.pipelineCache = &m_PipelineStateCache;
        passContext.frameData = &m_FrameData;
        passContext.gpuScene = &m_GPUScene;
        passContext.stats = &m_FrameData.stats;
        passContext.renderProxy = &m_RenderProxy;
        passContext.meshCache = &m_MeshGPUCache;
        passContext.assetManager = m_AssetManager;
        passContext.materialTextureCache = &m_MaterialTextureCache;
        passContext.shadowMap = m_ShadowPass.GetShadowMap();
        passContext.iblResources = m_IBLResources.IsReady() ? &m_IBLResources : nullptr;
        passContext.environmentExposureCompensation = m_SkyboxExposureCompensation;
        passContext.debugView = static_cast<std::uint32_t>(m_PostProcessSettings.debugView);
        passContext.lightingMode = ForwardLightingMode::Clustered;
        passContext.wireframe = m_PostProcessSettings.debugView == DebugViewMode::Wireframe;
        const auto passStart = std::chrono::steady_clock::now();
        m_ForwardOpaquePass.ExecuteUnlit(passContext);
        m_FrameData.stats.forwardOpaqueCpuMs += RendererDetail::ElapsedMilliseconds(passStart);
    }
}