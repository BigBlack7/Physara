#include "ForwardOpaquePass.hpp"

#include <array>
#include <cstddef>
#include <span>

#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/GPUContracts.hpp>
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
        constexpr std::uint32_t FrameUniformsBinding = Binding(GPUBufferBinding::FrameUniforms);
        constexpr std::uint32_t ObjectBinding = Binding(GPUBufferBinding::Objects);
        constexpr std::uint32_t MaterialBinding = Binding(GPUBufferBinding::Materials);
        constexpr std::uint32_t LightBinding = Binding(GPUBufferBinding::Lights);
        constexpr std::uint32_t InstanceObjectIndexBinding = Binding(GPUBufferBinding::InstanceIndices);
        constexpr std::uint32_t ShadowTextureBinding = Binding(GPUTextureBinding::ShadowMap);
        constexpr std::uint32_t IBLPrefilteredTextureBinding = Binding(GPUTextureBinding::IBLPrefiltered);
        constexpr std::uint32_t IBLBRDFLutBinding = Binding(GPUTextureBinding::IBLBRDFLut);
        constexpr std::uint32_t PerMaterialResourceSet = Binding(GPUResourceSetIndex::PerMaterial);

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);

        static bool CanMergeIndirectRun(const RenderCommand &lhs, const RenderCommand &rhs)
        {
            return lhs.materialInstanceId == rhs.materialInstanceId &&
                   lhs.meshKey == rhs.meshKey &&
                   lhs.bucket == rhs.bucket &&
                   lhs.doubleSided == rhs.doubleSided;
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
        m_MaterialTextureCache.Reset();
        m_LinearClampMipSampler.reset();
        m_ShadowSampler.reset();
        m_FallbackBlackCubeTexture.reset();
        m_FallbackBRDFLut.reset();
        m_CommandExecutor.Reset();
        ResetTextureBindings();
        m_LoggedFirstScene = false;
        m_LoggedFirstDraw = false;
    }

    void ForwardOpaquePass::ExecuteBuckets(const ForwardPassContext &context, bool transparent)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.renderProxy == nullptr || context.device == nullptr ||
            context.gpuScene == nullptr)
        {
            return;
        }

        EnsureDefaultTextures(context);
        m_CommandExecutor.BeginFrame();
        m_MaterialTextureCache.Update(*context.device, *context.commandList, context.assetManager, *context.frameData, context.stats);
        RHI::RHIPipelineState *singleSidedPipeline = GetPipeline(context, RHI::CullMode::Back, transparent);
        RHI::RHIPipelineState *doubleSidedPipeline = GetPipeline(context, RHI::CullMode::None, transparent);
        const FrameUploadAllocation &frameUniformAllocation = context.gpuScene->GetFrameUniformBuffer();
        const FrameUploadAllocation &objectAllocation = context.gpuScene->GetObjectBuffer();
        const FrameUploadAllocation &lightAllocation = context.gpuScene->GetLightBuffer();
        const FrameUploadAllocation &instanceObjectIndexAllocation = context.gpuScene->GetForwardInstanceObjectIndexBuffer();
        if (!frameUniformAllocation.IsValid() || !objectAllocation.IsValid() || !lightAllocation.IsValid() ||
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
                                  buckets.opaque.Size(),
                                  buckets.unlit.Size(),
                                  buckets.transparent.Size());
                m_LoggedFirstScene = true;
            }

            context.commandList->SetUniformBuffer(
                ForwardOpaquePassDetail::FrameUniformsBinding,
                frameUniformAllocation.buffer,
                frameUniformAllocation.offset,
                frameUniformAllocation.size);
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
            BindFrameTextures(context);

            ResetTextureBindings();
            const RenderCommandBuckets &commands = context.renderProxy->GetCommands();
            if (transparent)
            {
                DrawCommandGroup(
                    context,
                    singleSidedPipeline,
                    commands.transparent.singleSided,
                    std::span<const RenderCommand>{},
                    true);
            }
            else
            {
                DrawCommandGroup(
                    context,
                    singleSidedPipeline,
                    commands.opaque.singleSided,
                    commands.unlit.singleSided,
                    false);
            }

            if (transparent)
            {
                DrawCommandGroup(
                    context,
                    doubleSidedPipeline,
                    commands.transparent.doubleSided,
                    std::span<const RenderCommand>{},
                    true);
            }
            else
            {
                DrawCommandGroup(
                    context,
                    doubleSidedPipeline,
                    commands.opaque.doubleSided,
                    commands.unlit.doubleSided,
                    false);
            }
        }

        context.commandList->EndRenderPass();
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

    void ForwardOpaquePass::BindFrameTextures(const ForwardPassContext &context)
    {
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

    bool ForwardOpaquePass::BindMaterial(const ForwardPassContext &context, const RenderCommand &command)
    {
        if (context.frameData == nullptr || command.firstObjectIndex >= context.frameData->objects.size())
        {
            return false;
        }

        const std::uint32_t materialIndex = context.frameData->objects[command.firstObjectIndex].materialIndex;
        const MaterialResourceSet *resourceSet = m_MaterialTextureCache.GetResourceSet(materialIndex);
        if (resourceSet == nullptr)
        {
            return false;
        }

        if (m_BoundMaterialInstanceId == resourceSet->materialInstanceId)
        {
            return true;
        }

        context.commandList->SetResourceSet(ForwardOpaquePassDetail::PerMaterialResourceSet, resourceSet->AsRHIResourceSet());
        m_BoundMaterialInstanceId = resourceSet->materialInstanceId;
        return true;
    }

    void ForwardOpaquePass::DrawCommandGroup(
        const ForwardPassContext &context,
        RHI::RHIPipelineState *pipeline,
        std::span<const RenderCommand> primaryCommands,
        std::span<const RenderCommand> secondaryCommands,
        bool transparent)
    {
        if (pipeline == nullptr || (primaryCommands.empty() && secondaryCommands.empty()))
        {
            return;
        }

        context.commandList->SetPipelineState(pipeline);
        DrawCommands(context, primaryCommands, transparent);
        DrawCommands(context, secondaryCommands, transparent);
    }

    void ForwardOpaquePass::DrawCommands(const ForwardPassContext &context, std::span<const RenderCommand> commands, bool transparent)
    {
        CommandSubmitContext submitContext{this, &context, transparent};
        RenderCommandExecutorContext executorContext{};
        executorContext.device = context.device;
        executorContext.commandList = context.commandList;
        executorContext.meshCache = context.meshCache;
        executorContext.assetManager = context.assetManager;
        executorContext.stats = context.stats;
        RenderCommandSubmitCallbacks callbacks{};
        callbacks.userData = &submitContext;
        callbacks.canMergeIndirectRun = &ForwardOpaquePassDetail::CanMergeIndirectRun;
        callbacks.bindCommand = &ForwardOpaquePass::BindSubmittedCommand;
        callbacks.recordCommand = &ForwardOpaquePass::RecordSubmittedCommand;
        m_CommandExecutor.Submit(executorContext, commands, callbacks);
    }

    bool ForwardOpaquePass::BindSubmittedCommand(void *userData, const RenderCommand &command)
    {
        auto *submitContext = static_cast<CommandSubmitContext *>(userData);
        return submitContext != nullptr && submitContext->pass != nullptr && submitContext->passContext != nullptr &&
               submitContext->pass->BindMaterial(*submitContext->passContext, command);
    }

    void ForwardOpaquePass::RecordSubmittedCommand(
        void *userData,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode mode)
    {
        auto *submitContext = static_cast<CommandSubmitContext *>(userData);
        if (submitContext == nullptr || submitContext->pass == nullptr || submitContext->passContext == nullptr)
        {
            return;
        }

        submitContext->pass->RecordSubmittedCommand(
            *submitContext->passContext,
            command,
            primitive,
            mode,
            submitContext->transparent);
    }

    void ForwardOpaquePass::RecordSubmittedCommand(
        const ForwardPassContext &context,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode mode,
        bool transparent)
    {
        if (context.stats == nullptr)
        {
            return;
        }

        ++context.stats->drawCalls;
        if (transparent)
        {
            ++context.stats->forwardTransparentDrawCalls;
        }
        else
        {
            ++context.stats->forwardOpaqueDrawCalls;
        }
        context.stats->instances += command.instanceCount;
        context.stats->triangles += static_cast<std::uint64_t>(primitive.indexCount / 3u) * command.instanceCount;
        if (!m_LoggedFirstDraw)
        {
            PHYSARA_CORE_INFO("Forward {} submitted '{}': indices={}, objectIndex={}, instances={}.",
                              mode == RenderCommandSubmitMode::Indirect ? "MDI command" : "draw",
                              MeshGPUCache::BuildMeshPrimitiveDebugName(command),
                              primitive.indexCount,
                              command.firstObjectIndex,
                              command.instanceCount);
            m_LoggedFirstDraw = true;
        }
    }

    void ForwardOpaquePass::ResetTextureBindings()
    {
        m_BoundMaterialInstanceId = InvalidMaterialInstanceId;
    }
}