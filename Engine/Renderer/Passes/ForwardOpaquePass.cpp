#include "ForwardOpaquePass.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/IBLResources.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::Engine
{
    namespace ForwardOpaquePassDetail
    {
        constexpr std::uint32_t CameraBinding = 0u;
        constexpr std::uint32_t ObjectBinding = 1u;
        constexpr std::uint32_t MaterialBinding = 2u;
        constexpr std::uint32_t LightBinding = 3u;
        constexpr std::uint32_t InstanceObjectIndexBinding = 4u;
        constexpr std::uint32_t RenderSettingsBinding = 5u;
        constexpr std::uint32_t ShadowBinding = 6u;
        constexpr std::uint32_t IBLBinding = 7u;
        constexpr std::uint32_t BaseColorTextureBinding = 0u;
        constexpr std::uint32_t MetallicRoughnessTextureBinding = 1u;
        constexpr std::uint32_t NormalTextureBinding = 2u;
        constexpr std::uint32_t OcclusionTextureBinding = 3u;
        constexpr std::uint32_t EmissiveTextureBinding = 4u;
        constexpr std::uint32_t ShadowTextureBinding = 8u;
        constexpr std::uint32_t IBLPrefilteredTextureBinding = 9u;
        constexpr std::uint32_t IBLBRDFLutBinding = 10u;

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        struct alignas(16) RenderSettingsGPUData
        {
            glm::vec4 debugParams{0.f, 0.f, 0.f, 0.f};
        };
        static_assert(sizeof(RenderSettingsGPUData) % 16 == 0);

        struct alignas(16) IBLGPUData
        {
            glm::vec4 irradianceSH[9]{};
            glm::vec4 params{0.f, 0.f, 0.f, 0.f};
        };
        static_assert(sizeof(IBLGPUData) % 16 == 0);

        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);

        RenderSettingsGPUData BuildRenderSettings(const ForwardPassContext &context)
        {
            RenderSettingsGPUData data{};
            data.debugParams.x = static_cast<float>(context.debugView);
            return data;
        }

        IBLGPUData BuildIBLData(const ForwardPassContext &context)
        {
            IBLGPUData data{};
            if (context.iblResources == nullptr || !context.iblResources->IsReady())
            {
                return data;
            }

            const std::array<glm::vec4, 9> &sh = context.iblResources->GetIrradianceSH();
            for (std::size_t i = 0; i < sh.size(); ++i)
            {
                data.irradianceSH[i] = sh[i];
            }
            data.params = glm::vec4(
                std::exp2(context.environmentExposureCompensation),
                static_cast<float>(context.iblResources->GetSpecularMipCount() > 0u ? context.iblResources->GetSpecularMipCount() - 1u : 0u),
                1.f,
                0.f);
            return data;
        }

    }

    void ForwardOpaquePass::Execute(const ForwardPassContext &context)
    {
        ExecuteBuckets(context, false);
    }

    void ForwardOpaquePass::ExecuteTransparent(const ForwardPassContext &context)
    {
        ExecuteBuckets(context, true);
    }

    void ForwardOpaquePass::Reset()
    {
        m_CameraAllocation = {};
        m_RenderSettingsAllocation = {};
        m_ShadowAllocation = {};
        m_IBLAllocation = {};
        m_MaterialTextureCache.Reset();
        m_LinearClampMipSampler.reset();
        m_ShadowSampler.reset();
        m_FallbackBlackCubeTexture.reset();
        m_FallbackBRDFLut.reset();
        ResetTextureBindings();
        m_LastUploadedFrameIndex = std::numeric_limits<std::uint64_t>::max();
        m_LoggedFirstScene = false;
        m_LoggedFirstDraw = false;
    }

    void ForwardOpaquePass::ExecuteBuckets(const ForwardPassContext &context, bool transparent)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.renderProxy == nullptr || context.device == nullptr ||
            context.frameUploadAllocator == nullptr || context.gpuScene == nullptr)
        {
            return;
        }

        EnsureDefaultTextures(context);
        EnsureFrameBuffers(context);
        m_MaterialTextureCache.Update(*context.device, *context.commandList, context.assetManager, *context.frameData, context.stats);
        RHI::RHIPipelineState *singleSidedPipeline = GetPipeline(context, RHI::CullMode::Back, transparent);
        RHI::RHIPipelineState *doubleSidedPipeline = GetPipeline(context, RHI::CullMode::None, transparent);
        const FrameUploadAllocation &objectAllocation = context.gpuScene->GetObjectBuffer();
        const FrameUploadAllocation &lightAllocation = context.gpuScene->GetLightBuffer();
        const FrameUploadAllocation &instanceObjectIndexAllocation = context.gpuScene->GetForwardInstanceObjectIndexBuffer();
        if (!m_CameraAllocation.IsValid() || !m_RenderSettingsAllocation.IsValid() || !m_ShadowAllocation.IsValid() ||
            !m_IBLAllocation.IsValid() || !objectAllocation.IsValid() || !lightAllocation.IsValid() ||
            !instanceObjectIndexAllocation.IsValid() || context.gpuScene->GetMaterialBuffer() == nullptr)
        {
            return;
        }

        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        const std::array<glm::vec4, 1> clearColors{context.clearColor};
        context.commandList->BeginRenderPass(
            context.framebuffer,
            *context.renderPassDesc,
            transparent ? std::span<const glm::vec4>{} : std::span<const glm::vec4>{clearColors});

        if (singleSidedPipeline != nullptr && doubleSidedPipeline != nullptr)
        {
            if (!m_LoggedFirstScene && !context.frameData->objects.empty())
            {
                const RenderDrawBuckets &buckets = context.renderProxy->GetBuckets();
                PHYSARA_CORE_INFO("Forward pass scene data: objects={}, lights={}, opaque={}, unlit={}, transparent={}.",
                                  context.frameData->objects.size(),
                                  context.frameData->lights.size(),
                                  buckets.opaque.size(),
                                  buckets.unlit.size(),
                                  buckets.transparent.size());
                m_LoggedFirstScene = true;
            }

            context.commandList->SetUniformBuffer(
                ForwardOpaquePassDetail::CameraBinding,
                m_CameraAllocation.buffer,
                m_CameraAllocation.offset,
                m_CameraAllocation.size);
            context.commandList->SetStorageBuffer(
                ForwardOpaquePassDetail::ObjectBinding,
                objectAllocation.buffer,
                objectAllocation.offset,
                objectAllocation.size);
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::MaterialBinding, context.gpuScene->GetMaterialBuffer());
            context.commandList->SetStorageBuffer(
                ForwardOpaquePassDetail::LightBinding,
                lightAllocation.buffer,
                lightAllocation.offset,
                lightAllocation.size);
            context.commandList->SetStorageBuffer(
                ForwardOpaquePassDetail::InstanceObjectIndexBinding,
                instanceObjectIndexAllocation.buffer,
                instanceObjectIndexAllocation.offset,
                instanceObjectIndexAllocation.size);
            BindFrameState(context);

            ResetTextureBindings();
            context.commandList->SetPipelineState(singleSidedPipeline);
            const RenderDrawBatchBuckets &batches = context.renderProxy->GetBatches();
            if (transparent)
            {
                DrawBatches(context, batches.transparent, false, true);
            }
            else
            {
                DrawBatches(context, batches.opaque, false, false);
                DrawBatches(context, batches.unlit, false, false);
            }

            context.commandList->SetPipelineState(doubleSidedPipeline);
            if (transparent)
            {
                DrawBatches(context, batches.transparent, true, true);
            }
            else
            {
                DrawBatches(context, batches.opaque, true, false);
                DrawBatches(context, batches.unlit, true, false);
            }
        }

        context.commandList->EndRenderPass();
    }

    void ForwardOpaquePass::EnsureFrameBuffers(const ForwardPassContext &context)
    {
        const FrameData &frameData = *context.frameData;

        if (m_LastUploadedFrameIndex == frameData.frameIndex)
        {
            return;
        }

        const ForwardOpaquePassDetail::RenderSettingsGPUData renderSettings = ForwardOpaquePassDetail::BuildRenderSettings(context);
        const ForwardOpaquePassDetail::IBLGPUData iblData = ForwardOpaquePassDetail::BuildIBLData(context);
        m_CameraAllocation = context.frameUploadAllocator->Upload(*context.device, frameData.camera, context.stats);
        m_RenderSettingsAllocation = context.frameUploadAllocator->Upload(*context.device, renderSettings, context.stats);
        m_ShadowAllocation = context.frameUploadAllocator->Upload(*context.device, frameData.shadow, context.stats);
        m_IBLAllocation = context.frameUploadAllocator->Upload(*context.device, iblData, context.stats);
        m_LastUploadedFrameIndex = frameData.frameIndex;
    }

    void ForwardOpaquePass::EnsureDefaultTextures(const ForwardPassContext &context)
    {
        if (context.device == nullptr)
        {
            return;
        }

        if (m_LinearClampMipSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            desc.anisotropy = static_cast<float>(ForwardOpaquePassDetail::MaxValue(context.device->GetMaxAnisotropy(), 1));
            m_LinearClampMipSampler = context.device->CreateSampler(desc);
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
                for (std::uint32_t face = 0; face < 6u; ++face)
                {
                    m_FallbackBlackCubeTexture->Upload(0u, face, black, 0u);
                }
            }
        }

        if (m_FallbackBRDFLut == nullptr)
        {
            const float brdf[2]{0.f, 0.f};
            RHI::RHITextureDesc desc{};
            desc.width = 1u;
            desc.height = 1u;
            desc.mipLevels = 1u;
            desc.arrayLayers = 1u;
            desc.format = RHI::TextureFormat::RG16F;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = brdf;
            m_FallbackBRDFLut = context.device->CreateTexture(desc);
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
            desc.compareOp = RHI::CompareOp::None;
            desc.anisotropy = 1.f;
            m_ShadowSampler = context.device->CreateSampler(desc);
        }
    }

    RHI::RHIPipelineState *ForwardOpaquePass::GetPipeline(const ForwardPassContext &context, RHI::CullMode cullMode, bool transparent)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "Forward";
        shaderDesc.vertexPath = "Shaders/Passes/Forward/Forward.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/Forward/Forward.frag";

        ShaderVariant *variant = context.shaderLibrary->GetVariant(shaderDesc);
        if (variant == nullptr || !variant->IsValid())
        {
            return nullptr;
        }

        RHI::RHIPipelineStateDesc pipelineDesc{};
        pipelineDesc.vertexShader = variant->vertexShader.get();
        pipelineDesc.fragmentShader = variant->fragmentShader.get();
        pipelineDesc.renderPassDesc = context.renderPassDesc;
        pipelineDesc.vertexBindings.push_back({0u, ForwardOpaquePassDetail::VertexStride, 0u});
        pipelineDesc.vertexAttributes.push_back({0u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, position))});
        pipelineDesc.vertexAttributes.push_back({1u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, normal))});
        pipelineDesc.vertexAttributes.push_back({2u, 0u, RHI::VertexFormat::RGBA32F, static_cast<std::uint32_t>(offsetof(MeshVertex, tangent))});
        pipelineDesc.vertexAttributes.push_back({3u, 0u, RHI::VertexFormat::RG32F, static_cast<std::uint32_t>(offsetof(MeshVertex, texCoord0))});
        pipelineDesc.vertexAttributes.push_back({4u, 0u, RHI::VertexFormat::RG32F, static_cast<std::uint32_t>(offsetof(MeshVertex, texCoord1))});
        pipelineDesc.rasterizerState.cullMode = cullMode;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = !transparent;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        RHI::RHIBlendState blendState{};
        if (transparent)
        {
            blendState.blendEnable = true;
            blendState.srcColor = RHI::BlendFactor::SrcAlpha;
            blendState.dstColor = RHI::BlendFactor::OneMinusSrcAlpha;
            blendState.srcAlpha = RHI::BlendFactor::One;
            blendState.dstAlpha = RHI::BlendFactor::OneMinusSrcAlpha;
        }
        pipelineDesc.blendStates.push_back(blendState);
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    void ForwardOpaquePass::BindFrameState(const ForwardPassContext &context)
    {
        context.commandList->SetUniformBuffer(
            ForwardOpaquePassDetail::RenderSettingsBinding,
            m_RenderSettingsAllocation.buffer,
            m_RenderSettingsAllocation.offset,
            m_RenderSettingsAllocation.size);
        context.commandList->SetUniformBuffer(
            ForwardOpaquePassDetail::ShadowBinding,
            m_ShadowAllocation.buffer,
            m_ShadowAllocation.offset,
            m_ShadowAllocation.size);
        context.commandList->SetUniformBuffer(
            ForwardOpaquePassDetail::IBLBinding,
            m_IBLAllocation.buffer,
            m_IBLAllocation.offset,
            m_IBLAllocation.size);
        if (context.shadowMap != nullptr)
        {
            context.commandList->SetTexture(ForwardOpaquePassDetail::ShadowTextureBinding, context.shadowMap, m_ShadowSampler.get());
        }
        RHI::RHITexture *iblSpecular = context.iblResources != nullptr && context.iblResources->IsReady()
                                           ? context.iblResources->GetSpecularTexture()
                                           : m_FallbackBlackCubeTexture.get();
        RHI::RHITexture *iblBRDF = context.iblResources != nullptr && context.iblResources->IsReady()
                                       ? context.iblResources->GetBRDFLut()
                                       : m_FallbackBRDFLut.get();
        if (iblSpecular != nullptr && iblBRDF != nullptr)
        {
            context.commandList->SetTexture(ForwardOpaquePassDetail::IBLPrefilteredTextureBinding, iblSpecular, m_LinearClampMipSampler.get());
            context.commandList->SetTexture(ForwardOpaquePassDetail::IBLBRDFLutBinding, iblBRDF, m_LinearClampMipSampler.get());
        }
    }

    void ForwardOpaquePass::BindMaterial(const ForwardPassContext &context, const RenderDrawBatch &batch)
    {
        if (context.frameData == nullptr || batch.firstObjectIndex >= context.frameData->objects.size())
        {
            return;
        }

        const std::uint32_t materialIndex = context.frameData->objects[batch.firstObjectIndex].materialIndex;
        if (m_BoundMaterialIndex == materialIndex)
        {
            return;
        }

        const MaterialTextureBinding *materialBinding = m_MaterialTextureCache.GetBinding(materialIndex);
        if (materialBinding == nullptr)
        {
            return;
        }

        const std::uint32_t bindings[5]{
            ForwardOpaquePassDetail::BaseColorTextureBinding,
            ForwardOpaquePassDetail::MetallicRoughnessTextureBinding,
            ForwardOpaquePassDetail::NormalTextureBinding,
            ForwardOpaquePassDetail::OcclusionTextureBinding,
            ForwardOpaquePassDetail::EmissiveTextureBinding};

        RHI::RHISampler *sampler = materialBinding->sampler;
        for (std::size_t i = 0; i < 5u; ++i)
        {
            RHI::RHITexture *texture = materialBinding->textures[i];
            if (m_BoundTextures[i] == texture && m_BoundSampler == sampler)
            {
                continue;
            }

            context.commandList->SetTexture(bindings[i], texture, sampler);
            m_BoundTextures[i] = texture;
        }
        m_BoundSampler = sampler;
        m_BoundMaterialIndex = materialIndex;
    }

    void ForwardOpaquePass::DrawBatches(const ForwardPassContext &context, const std::vector<RenderDrawBatch> &batches, bool drawDoubleSided, bool transparent)
    {
        for (const RenderDrawBatch &batch : batches)
        {
            if (batch.doubleSided != drawDoubleSided)
            {
                continue;
            }

            RenderDrawItem item{};
            item.submission = batch.submission;
            item.objectIndex = batch.firstObjectIndex;
            item.sortKey = batch.sortKey;
            item.meshKey = batch.meshKey;
            item.primitiveKey = batch.primitiveKey;
            item.doubleSided = batch.doubleSided;

            MeshGPUPrimitive *primitive = context.meshCache != nullptr
                                              ? context.meshCache->GetOrCreate(context.device, context.assetManager, item, context.stats)
                                              : nullptr;
            if (primitive == nullptr || primitive->indexCount == 0)
            {
                continue;
            }

            const std::uint32_t instanceCount = batch.itemCount;
            BindMaterial(context, batch);
            context.commandList->SetVertexBuffer(0u, primitive->vertexBuffer);
            context.commandList->SetIndexBuffer(primitive->indexBuffer);
            context.commandList->DrawIndexed(primitive->indexCount, instanceCount, primitive->firstIndex, primitive->vertexOffset, batch.firstInstanceIndex);
            if (context.stats != nullptr)
            {
                ++context.stats->drawCalls;
                if (transparent)
                {
                    ++context.stats->forwardTransparentDrawCalls;
                }
                else
                {
                    ++context.stats->forwardOpaqueDrawCalls;
                }
                context.stats->instances += instanceCount;
                context.stats->triangles += static_cast<std::uint64_t>(primitive->indexCount / 3u) * instanceCount;
            }
            if (!m_LoggedFirstDraw)
            {
                PHYSARA_CORE_INFO("Forward draw submitted '{}': indices={}, objectIndex={}, instances={}.",
                                  MeshGPUCache::BuildMeshPrimitiveDebugName(item),
                                  primitive->indexCount,
                                  batch.firstObjectIndex,
                                  instanceCount);
                m_LoggedFirstDraw = true;
            }
        }
    }

    void ForwardOpaquePass::ResetTextureBindings()
    {
        for (RHI::RHITexture *&texture : m_BoundTextures)
        {
            texture = nullptr;
        }
        m_BoundSampler = nullptr;
        m_BoundMaterialIndex = std::numeric_limits<std::uint32_t>::max();
    }

}