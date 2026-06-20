#include "DeferredLightingPass.hpp"

#include <array>

#include <glm/vec4.hpp>

#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/IBLResources.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::Engine
{
    namespace DeferredLightingPassDetail
    {
        constexpr std::uint32_t FrameUniformsBinding = Binding(GPUBufferBinding::FrameUniforms);
        constexpr std::uint32_t LightBinding = Binding(GPUBufferBinding::Lights);
        constexpr std::uint32_t ClusterEntryBinding = Binding(GPUBufferBinding::ClusterEntries);
        constexpr std::uint32_t ClusterLightIndexBinding = Binding(GPUBufferBinding::ClusterLightIndices);
        constexpr std::uint32_t SceneDepthBinding = Binding(GPUTextureBinding::SceneDepth);
        constexpr std::uint32_t ShadowMapBinding = Binding(GPUTextureBinding::ShadowMap);
        constexpr std::uint32_t IBLPrefilteredBinding = Binding(GPUTextureBinding::IBLPrefiltered);
        constexpr std::uint32_t IBLBRDFLutBinding = Binding(GPUTextureBinding::IBLBRDFLut);
        constexpr std::uint32_t BaseColorBinding = Binding(GPUTextureBinding::GBufferBaseColor);
        constexpr std::uint32_t NormalBinding = Binding(GPUTextureBinding::GBufferNormal);
        constexpr std::uint32_t MaterialBinding = Binding(GPUTextureBinding::GBufferMaterial);
        constexpr std::uint32_t EmissiveBinding = Binding(GPUTextureBinding::GBufferEmissive);
    }

    void DeferredLightingPass::Execute(const DeferredLightingPassContext &context)
    {
        if (context.commandList == nullptr || context.device == nullptr || context.framebuffer == nullptr ||
            context.renderPassDesc == nullptr || context.frameData == nullptr || context.gpuScene == nullptr ||
            context.sceneDepth == nullptr || context.baseColor == nullptr || context.normal == nullptr ||
            context.material == nullptr || context.emissive == nullptr)
        {
            return;
        }

        EnsureResources(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        const FrameUploadAllocation &frameUniforms = context.gpuScene->GetFrameUniformBuffer();
        const FrameUploadAllocation &lights = context.gpuScene->GetLightBuffer();
        const FrameUploadAllocation &clusterEntries = context.gpuScene->GetClusterEntryBuffer();
        const FrameUploadAllocation &clusterLightIndices = context.gpuScene->GetClusterLightIndexBuffer();
        if (pipeline == nullptr || !frameUniforms.IsValid() || !lights.IsValid() ||
            !clusterEntries.IsValid() || !clusterLightIndices.IsValid())
        {
            return;
        }

        const std::array<glm::vec4, 1> clearColors{context.clearColor};
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, clearColors);
        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(
            DeferredLightingPassDetail::FrameUniformsBinding,
            frameUniforms.buffer,
            frameUniforms.offset,
            frameUniforms.size);
        context.commandList->SetStorageBuffer(
            DeferredLightingPassDetail::LightBinding,
            lights.buffer,
            lights.offset,
            lights.size);
        context.commandList->SetStorageBuffer(
            DeferredLightingPassDetail::ClusterEntryBinding,
            clusterEntries.buffer,
            clusterEntries.offset,
            clusterEntries.size);
        context.commandList->SetStorageBuffer(
            DeferredLightingPassDetail::ClusterLightIndexBinding,
            clusterLightIndices.buffer,
            clusterLightIndices.offset,
            clusterLightIndices.size);
        context.commandList->SetTexture(DeferredLightingPassDetail::SceneDepthBinding, context.sceneDepth, m_NearestClampSampler.get());
        context.commandList->SetTexture(DeferredLightingPassDetail::BaseColorBinding, context.baseColor, m_NearestClampSampler.get());
        context.commandList->SetTexture(DeferredLightingPassDetail::NormalBinding, context.normal, m_NearestClampSampler.get());
        context.commandList->SetTexture(DeferredLightingPassDetail::MaterialBinding, context.material, m_NearestClampSampler.get());
        context.commandList->SetTexture(DeferredLightingPassDetail::EmissiveBinding, context.emissive, m_NearestClampSampler.get());
        if (context.shadowMap != nullptr)
        {
            context.commandList->SetTexture(DeferredLightingPassDetail::ShadowMapBinding, context.shadowMap, m_ShadowSampler.get());
        }
        RHI::RHITexture *iblSpecular = context.iblResources != nullptr && context.iblResources->IsReady()
                                           ? context.iblResources->GetSpecularTexture()
                                           : m_FallbackBlackCubeTexture.get();
        RHI::RHITexture *iblBRDF = context.iblResources != nullptr && context.iblResources->IsReady()
                                       ? context.iblResources->GetBRDFLut()
                                       : m_FallbackBRDFLut.get();
        context.commandList->SetTexture(DeferredLightingPassDetail::IBLPrefilteredBinding, iblSpecular, m_LinearClampSampler.get());
        context.commandList->SetTexture(DeferredLightingPassDetail::IBLBRDFLutBinding, iblBRDF, m_LinearClampSampler.get());
        context.commandList->Draw(3u, 1u, 0u, 0u);
        if (context.stats != nullptr)
        {
            ++context.stats->drawCalls;
            ++context.stats->deferredLightingDrawCalls;
            ++context.stats->instances;
            ++context.stats->triangles;
        }
        context.commandList->EndRenderPass();
    }

    void DeferredLightingPass::Reset()
    {
        m_LinearClampSampler.reset();
        m_NearestClampSampler.reset();
        m_ShadowSampler.reset();
        m_FallbackBlackCubeTexture.reset();
        m_FallbackBRDFLut.reset();
    }

    void DeferredLightingPass::EnsureResources(const DeferredLightingPassContext &context)
    {
        if (m_LinearClampSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            m_LinearClampSampler = context.device->CreateSampler(desc);
        }
        if (m_NearestClampSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Nearest;
            desc.magFilter = RHI::FilterMode::Nearest;
            desc.mipFilter = RHI::FilterMode::Nearest;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            m_NearestClampSampler = context.device->CreateSampler(desc);
        }
        if (m_ShadowSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Nearest;
            desc.magFilter = RHI::FilterMode::Nearest;
            desc.mipFilter = RHI::FilterMode::Nearest;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            m_ShadowSampler = context.device->CreateSampler(desc);
        }
        if (m_FallbackBlackCubeTexture == nullptr)
        {
            const float black[4]{0.f, 0.f, 0.f, 1.f};
            RHI::RHITextureDesc desc{};
            desc.width = 1u;
            desc.height = 1u;
            desc.mipLevels = 1u;
            desc.arrayLayers = 6u;
            desc.format = RHI::TextureFormat::RGBA16F;
            desc.dimension = RHI::TextureDimension::TexCube;
            desc.usage = RHI::TextureUsage::Sampled;
            m_FallbackBlackCubeTexture = context.device->CreateTexture(desc);
            if (m_FallbackBlackCubeTexture != nullptr)
            {
                for (std::uint32_t face = 0u; face < 6u; ++face)
                {
                    m_FallbackBlackCubeTexture->Upload(0u, face, black, 0u);
                }
            }
        }
        if (m_FallbackBRDFLut == nullptr)
        {
            const float value[2]{0.f, 0.f};
            RHI::RHITextureDesc desc{};
            desc.width = 1u;
            desc.height = 1u;
            desc.format = RHI::TextureFormat::RG16F;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = value;
            m_FallbackBRDFLut = context.device->CreateTexture(desc);
        }
    }

    RHI::RHIPipelineState *DeferredLightingPass::GetPipeline(const DeferredLightingPassContext &context)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "DeferredLighting";
        shaderDesc.vertexPath = "Shaders/Passes/PostProcess/Composite.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/Deferred/DeferredLighting.frag";
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
}