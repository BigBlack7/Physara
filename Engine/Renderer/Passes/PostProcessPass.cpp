#include "PostProcessPass.hpp"

#include <algorithm>
#include <array>

#include <glm/vec4.hpp>

#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/UploadHasher.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::Engine
{
    namespace PostProcessPassDetail
    {
        constexpr std::uint32_t CameraBinding = 0u;
        constexpr std::uint32_t SettingsBinding = 4u;
        constexpr std::uint32_t SceneColorBinding = 6u;
        constexpr std::uint32_t SceneDepthBinding = 7u;
        constexpr std::uint32_t BloomTextureBinding = 11u;
        constexpr std::uint32_t MaxBloomMips = 7u;

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        struct SettingsGPUData
        {
            glm::vec4 bloomParams{1.f, 0.5f, 0.12f, 2.f};
            glm::vec4 flags{1.f, 1.f, 2.f, 0.f};
            glm::vec4 exposureParams{0.f, 0.f, 0.f, 0.f};
            glm::vec4 aaParams{0.75f, 0.125f, 0.0312f, 24.f};
        };

        struct FrameGPUData
        {
            glm::vec4 viewportSizeEV100{1.f, 1.f, 0.f, 0.f};
            glm::vec4 clipPlanes{0.1f, 1000.f, 0.f, 0.f};
        };

        RHI::RHIBufferDesc DynamicBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = MaxValue(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        SettingsGPUData BuildSettings(const PostProcessSettings &settings)
        {
            SettingsGPUData data{};
            data.bloomParams = glm::vec4(
                std::max(settings.bloomThreshold, 0.f),
                std::max(settings.bloomKnee, 0.f),
                std::max(settings.bloomIntensity, 0.f),
                std::max(settings.bloomRadius, 1.f));
            data.flags = glm::vec4(
                static_cast<float>(settings.toneMappingMode),
                settings.bloomEnabled ? 1.f : 0.f,
                static_cast<float>(settings.antiAliasingMode),
                static_cast<float>(settings.debugView));
            data.exposureParams = glm::vec4(
                settings.exposureMode == ExposureMode::Auto ? 1.f : 0.f,
                settings.exposureCompensationEV,
                static_cast<float>(settings.bloomMode),
                0.f);
            data.aaParams = glm::vec4(
                std::clamp(settings.aaSubpixel, 0.f, 1.f),
                std::max(settings.aaEdgeThreshold, 0.001f),
                std::max(settings.aaEdgeThresholdMin, 0.0001f),
                std::max(settings.aaDepthSensitivity, 0.f));
            return data;
        }

        FrameGPUData BuildFrameData(const FrameData &frameData)
        {
            FrameGPUData data{};
            data.viewportSizeEV100 = glm::vec4(
                static_cast<float>(frameData.view.viewport.width),
                static_cast<float>(frameData.view.viewport.height),
                frameData.view.ev100,
                0.f);
            data.clipPlanes = glm::vec4(
                frameData.view.nearClipMeters,
                frameData.view.farClipMeters,
                0.f,
                0.f);
            return data;
        }
    }

    void PostProcessPass::Execute(const PostProcessPassContext &context)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.sceneHDR == nullptr || context.sceneDepth == nullptr || context.device == nullptr)
        {
            return;
        }

        EnsureResources(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        if (pipeline == nullptr)
        {
            return;
        }

        const PostProcessPassDetail::FrameGPUData frameData = PostProcessPassDetail::BuildFrameData(*context.frameData);
        const PostProcessPassDetail::SettingsGPUData settingsData = PostProcessPassDetail::BuildSettings(context.settings);
        const std::uint64_t frameSignature = UploadHash::Value(UploadHash::Offset, frameData);
        if (frameSignature != m_LastFrameUploadSignature)
        {
            m_FrameBuffer->UploadData(&frameData, sizeof(frameData));
            if (context.stats != nullptr)
            {
                context.stats->bufferUploadBytes += sizeof(frameData);
            }
            m_LastFrameUploadSignature = frameSignature;
        }

        const std::uint64_t settingsSignature = UploadHash::Value(UploadHash::Offset, settingsData);
        if (settingsSignature != m_LastSettingsUploadSignature)
        {
            m_SettingsBuffer->UploadData(&settingsData, sizeof(settingsData));
            if (context.stats != nullptr)
            {
                context.stats->bufferUploadBytes += sizeof(settingsData);
            }
            m_LastSettingsUploadSignature = settingsSignature;
        }

        ExecuteBloom(context);

        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        const std::array<glm::vec4, 1> clearColors{glm::vec4(0.f, 0.f, 0.f, 1.f)};
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, clearColors);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(PostProcessPassDetail::CameraBinding, m_FrameBuffer.get());
        context.commandList->SetUniformBuffer(PostProcessPassDetail::SettingsBinding, m_SettingsBuffer.get());
        context.commandList->SetTexture(PostProcessPassDetail::SceneColorBinding, context.sceneHDR, m_LinearClampSampler.get());
        context.commandList->SetTexture(PostProcessPassDetail::SceneDepthBinding, context.sceneDepth, m_LinearClampSampler.get());
        context.commandList->SetTexture(PostProcessPassDetail::BloomTextureBinding, GetBloomTexture(), m_LinearClampSampler.get());
        context.commandList->Draw(3u, 1u, 0u, 0u);
        if (context.stats != nullptr)
        {
            ++context.stats->drawCalls;
            ++context.stats->instances;
            ++context.stats->triangles;
        }
        context.commandList->EndRenderPass();
    }

    void PostProcessPass::EnsureResources(const PostProcessPassContext &context)
    {
        if (m_FrameBuffer == nullptr)
        {
            m_FrameBuffer = context.device->CreateBuffer(
                PostProcessPassDetail::DynamicBufferDesc(sizeof(PostProcessPassDetail::FrameGPUData), RHI::BufferUsage::Uniform));
            m_LastFrameUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        if (m_SettingsBuffer == nullptr)
        {
            m_SettingsBuffer = context.device->CreateBuffer(
                PostProcessPassDetail::DynamicBufferDesc(sizeof(PostProcessPassDetail::SettingsGPUData), RHI::BufferUsage::Uniform));
            m_LastSettingsUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        if (m_LinearClampSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            desc.anisotropy = 1.f;
            m_LinearClampSampler = context.device->CreateSampler(desc);
        }

        if (m_BlackTexture == nullptr)
        {
            const float black[4]{0.f, 0.f, 0.f, 1.f};
            RHI::RHITextureDesc desc{};
            desc.width = 1u;
            desc.height = 1u;
            desc.format = RHI::TextureFormat::RGBA16F;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = black;
            m_BlackTexture = context.device->CreateTexture(desc);
        }
    }

    void PostProcessPass::EnsureBloomResources(const PostProcessPassContext &context)
    {
        const std::uint32_t width = context.frameData != nullptr ? context.frameData->view.viewport.width : 0u;
        const std::uint32_t height = context.frameData != nullptr ? context.frameData->view.viewport.height : 0u;
        if (context.device == nullptr || width == 0u || height == 0u)
        {
            m_BloomMips.clear();
            m_BloomWidth = 0u;
            m_BloomHeight = 0u;
            return;
        }

        if (!m_BloomMips.empty() && m_BloomWidth == width && m_BloomHeight == height)
        {
            return;
        }

        m_BloomWidth = width;
        m_BloomHeight = height;
        m_BloomMips.clear();
        m_BloomRenderPassDesc = {};
        m_BloomRenderPassDesc.colorAttachments.push_back({
            RHI::TextureFormat::RGBA16F,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            1u});
        m_BloomRenderPassDesc.hasDepth = false;

        std::uint32_t mipWidth = PostProcessPassDetail::MaxValue(width / 2u, 1u);
        std::uint32_t mipHeight = PostProcessPassDetail::MaxValue(height / 2u, 1u);
        for (std::uint32_t i = 0; i < PostProcessPassDetail::MaxBloomMips && mipWidth >= 2u && mipHeight >= 2u; ++i)
        {
            BloomMip mip{};
            mip.width = mipWidth;
            mip.height = mipHeight;

            RHI::RHITextureDesc textureDesc{};
            textureDesc.width = mipWidth;
            textureDesc.height = mipHeight;
            textureDesc.format = RHI::TextureFormat::RGBA16F;
            textureDesc.dimension = RHI::TextureDimension::Tex2D;
            textureDesc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::RenderTarget;
            textureDesc.mipLevels = 1u;
            textureDesc.arrayLayers = 1u;
            textureDesc.samples = 1u;
            mip.downTexture = context.device->CreateTexture(textureDesc);
            mip.upTexture = context.device->CreateTexture(textureDesc);
            if (mip.downTexture == nullptr || mip.upTexture == nullptr)
            {
                m_BloomMips.clear();
                return;
            }

            RHI::RHIFramebufferDesc downFramebufferDesc{};
            downFramebufferDesc.colorAttachments.push_back(mip.downTexture.get());
            downFramebufferDesc.width = mipWidth;
            downFramebufferDesc.height = mipHeight;
            downFramebufferDesc.renderPassDesc = &m_BloomRenderPassDesc;
            mip.downFramebuffer = context.device->CreateFramebuffer(downFramebufferDesc);

            RHI::RHIFramebufferDesc upFramebufferDesc{};
            upFramebufferDesc.colorAttachments.push_back(mip.upTexture.get());
            upFramebufferDesc.width = mipWidth;
            upFramebufferDesc.height = mipHeight;
            upFramebufferDesc.renderPassDesc = &m_BloomRenderPassDesc;
            mip.upFramebuffer = context.device->CreateFramebuffer(upFramebufferDesc);

            if (mip.downFramebuffer == nullptr || mip.upFramebuffer == nullptr)
            {
                m_BloomMips.clear();
                return;
            }

            m_BloomMips.push_back(std::move(mip));
            mipWidth = PostProcessPassDetail::MaxValue(mipWidth / 2u, 1u);
            mipHeight = PostProcessPassDetail::MaxValue(mipHeight / 2u, 1u);
        }
    }

    void PostProcessPass::ExecuteBloom(const PostProcessPassContext &context)
    {
        if (!context.settings.bloomEnabled || context.settings.bloomMode == BloomMode::Legacy || context.commandList == nullptr ||
            context.sceneHDR == nullptr || context.device == nullptr || context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return;
        }

        EnsureBloomResources(context);
        if (m_BloomMips.empty())
        {
            return;
        }

        RHI::RHIPipelineState *prefilterPipeline = GetBloomPipeline(context, "BloomPrefilter", "Shaders/Passes/PostProcess/BloomPrefilter.frag");
        const bool kawase = context.settings.bloomMode == BloomMode::DualKawase;
        RHI::RHIPipelineState *downPipeline = GetBloomPipeline(
            context,
            kawase ? "BloomKawaseDownsample" : "BloomDownsample",
            kawase ? "Shaders/Passes/PostProcess/BloomKawaseDownsample.frag" : "Shaders/Passes/PostProcess/BloomDownsample.frag");
        RHI::RHIPipelineState *copyPipeline = GetBloomPipeline(context, "BloomCopy", "Shaders/Passes/PostProcess/BloomCopy.frag");
        RHI::RHIPipelineState *upPipeline = GetBloomPipeline(
            context,
            kawase ? "BloomKawaseUpsample" : "BloomUpsample",
            kawase ? "Shaders/Passes/PostProcess/BloomKawaseUpsample.frag" : "Shaders/Passes/PostProcess/BloomUpsample.frag");
        if (prefilterPipeline == nullptr || downPipeline == nullptr || copyPipeline == nullptr || upPipeline == nullptr)
        {
            return;
        }

        ExecuteFullscreenPass(
            context,
            m_BloomMips.front().downFramebuffer.get(),
            m_BloomRenderPassDesc,
            prefilterPipeline,
            m_BloomMips.front().width,
            m_BloomMips.front().height,
            context.sceneHDR,
            m_BlackTexture.get());

        for (std::size_t i = 1u; i < m_BloomMips.size(); ++i)
        {
            context.commandList->TextureBarrier(m_BloomMips[i - 1u].downTexture.get(), RHI::ShaderStage::Fragment, RHI::ShaderStage::Fragment);
            ExecuteFullscreenPass(
                context,
                m_BloomMips[i].downFramebuffer.get(),
                m_BloomRenderPassDesc,
                downPipeline,
                m_BloomMips[i].width,
                m_BloomMips[i].height,
                m_BloomMips[i - 1u].downTexture.get(),
                m_BlackTexture.get());
        }

        const std::size_t last = m_BloomMips.size() - 1u;
        context.commandList->TextureBarrier(m_BloomMips[last].downTexture.get(), RHI::ShaderStage::Fragment, RHI::ShaderStage::Fragment);
        ExecuteFullscreenPass(
            context,
            m_BloomMips[last].upFramebuffer.get(),
            m_BloomRenderPassDesc,
            copyPipeline,
            m_BloomMips[last].width,
            m_BloomMips[last].height,
            m_BloomMips[last].downTexture.get(),
            m_BlackTexture.get());

        for (std::size_t reverseIndex = last; reverseIndex > 0u; --reverseIndex)
        {
            const std::size_t i = reverseIndex - 1u;
            context.commandList->TextureBarrier(m_BloomMips[i].downTexture.get(), RHI::ShaderStage::Fragment, RHI::ShaderStage::Fragment);
            context.commandList->TextureBarrier(m_BloomMips[i + 1u].upTexture.get(), RHI::ShaderStage::Fragment, RHI::ShaderStage::Fragment);
            ExecuteFullscreenPass(
                context,
                m_BloomMips[i].upFramebuffer.get(),
                m_BloomRenderPassDesc,
                upPipeline,
                m_BloomMips[i].width,
                m_BloomMips[i].height,
                m_BloomMips[i].downTexture.get(),
                m_BloomMips[i + 1u].upTexture.get());
        }

        context.commandList->TextureBarrier(m_BloomMips.front().upTexture.get(), RHI::ShaderStage::Fragment, RHI::ShaderStage::Fragment);
    }

    void PostProcessPass::ExecuteFullscreenPass(
        const PostProcessPassContext &context,
        RHI::RHIFramebuffer *framebuffer,
        const RHI::RHIRenderPassDesc &renderPassDesc,
        RHI::RHIPipelineState *pipeline,
        std::uint32_t width,
        std::uint32_t height,
        RHI::RHITexture *source0,
        RHI::RHITexture *source1)
    {
        if (framebuffer == nullptr || pipeline == nullptr || context.commandList == nullptr)
        {
            return;
        }

        context.commandList->SetViewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height));
        context.commandList->SetScissor(0, 0, width, height);
        const std::array<glm::vec4, 1> clearColors{glm::vec4(0.f, 0.f, 0.f, 1.f)};
        context.commandList->BeginRenderPass(framebuffer, renderPassDesc, clearColors);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(PostProcessPassDetail::CameraBinding, m_FrameBuffer.get());
        context.commandList->SetUniformBuffer(PostProcessPassDetail::SettingsBinding, m_SettingsBuffer.get());
        context.commandList->SetTexture(PostProcessPassDetail::SceneColorBinding, source0, m_LinearClampSampler.get());
        context.commandList->SetTexture(PostProcessPassDetail::SceneDepthBinding, source1, m_LinearClampSampler.get());
        context.commandList->Draw(3u, 1u, 0u, 0u);
        if (context.stats != nullptr)
        {
            ++context.stats->drawCalls;
            ++context.stats->instances;
            ++context.stats->triangles;
        }
        context.commandList->EndRenderPass();
    }

    RHI::RHIPipelineState *PostProcessPass::GetPipeline(const PostProcessPassContext &context)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "PostProcessComposite";
        shaderDesc.vertexPath = "Shaders/Passes/PostProcess/Composite.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/PostProcess/Composite.frag";

        ShaderVariant *variant = context.shaderLibrary->GetVariant(shaderDesc);
        if (variant == nullptr || !variant->IsValid())
        {
            return nullptr;
        }

        RHI::RHIPipelineStateDesc pipelineDesc{};
        pipelineDesc.vertexShader = variant->vertexShader.get();
        pipelineDesc.fragmentShader = variant->fragmentShader.get();
        pipelineDesc.renderPassDesc = context.renderPassDesc;
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::None;
        pipelineDesc.depthStencilState.depthTest = false;
        pipelineDesc.depthStencilState.depthWrite = false;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Always;
        pipelineDesc.blendStates.push_back({});
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    RHI::RHIPipelineState *PostProcessPass::GetBloomPipeline(
        const PostProcessPassContext &context,
        const char *debugName,
        const char *fragmentPath)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = debugName;
        shaderDesc.vertexPath = "Shaders/Passes/PostProcess/Composite.vert";
        shaderDesc.fragmentPath = fragmentPath;

        ShaderVariant *variant = context.shaderLibrary->GetVariant(shaderDesc);
        if (variant == nullptr || !variant->IsValid())
        {
            return nullptr;
        }

        RHI::RHIPipelineStateDesc pipelineDesc{};
        pipelineDesc.vertexShader = variant->vertexShader.get();
        pipelineDesc.fragmentShader = variant->fragmentShader.get();
        pipelineDesc.renderPassDesc = &m_BloomRenderPassDesc;
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::None;
        pipelineDesc.depthStencilState.depthTest = false;
        pipelineDesc.depthStencilState.depthWrite = false;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Always;
        pipelineDesc.blendStates.push_back({});
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    RHI::RHITexture *PostProcessPass::GetBloomTexture() const
    {
        return !m_BloomMips.empty() && m_BloomMips.front().upTexture != nullptr ? m_BloomMips.front().upTexture.get() : m_BlackTexture.get();
    }
}