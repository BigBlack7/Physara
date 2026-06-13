#include "ForwardOpaquePass.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

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
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
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
        constexpr std::uint32_t MinIndirectRunCommandCount = 2u;

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
        m_IndirectCommandBuffers.clear();
        m_IndirectCommandScratch.clear();
        m_IndirectPrimitiveScratch.clear();
        m_IndirectRuns.clear();
        m_IndirectBufferCursor = 0;
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
        m_IndirectBufferCursor = 0;
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
        if (commands.empty())
        {
            return;
        }

        BuildIndirectRuns(context, commands);
        std::uint32_t runIndex = 0;
        for (std::uint32_t commandIndex = 0; commandIndex < static_cast<std::uint32_t>(commands.size());)
        {
            if (runIndex < m_IndirectRuns.size() && m_IndirectRuns[runIndex].commandIndex == commandIndex)
            {
                const IndirectRun &run = m_IndirectRuns[runIndex++];
                SubmitIndirectRun(context, commands, run, transparent);
                commandIndex += run.commandCount;
                continue;
            }

            SubmitDirectCommand(context, commands[commandIndex], transparent);
            ++commandIndex;
        }
    }

    void ForwardOpaquePass::BuildIndirectRuns(const ForwardPassContext &context, std::span<const RenderCommand> commands)
    {
        m_IndirectRuns.clear();
        m_IndirectCommandScratch.clear();
        m_IndirectPrimitiveScratch.clear();

        if (commands.size() < ForwardOpaquePassDetail::MinIndirectRunCommandCount || context.meshCache == nullptr ||
            context.device == nullptr)
        {
            return;
        }

        for (const RenderCommand &command : commands)
        {
            [[maybe_unused]] MeshGPUPrimitive *primitive =
                context.meshCache->GetOrCreate(context.device, context.assetManager, command, context.stats);
        }

        for (std::uint32_t commandIndex = 0; commandIndex < static_cast<std::uint32_t>(commands.size());)
        {
            const RenderCommand &firstCommand = commands[commandIndex];
            std::uint32_t runCommandCount = 1u;
            while (commandIndex + runCommandCount < static_cast<std::uint32_t>(commands.size()) &&
                   ForwardOpaquePassDetail::CanMergeIndirectRun(firstCommand, commands[commandIndex + runCommandCount]))
            {
                ++runCommandCount;
            }

            if (runCommandCount < ForwardOpaquePassDetail::MinIndirectRunCommandCount)
            {
                commandIndex += runCommandCount;
                continue;
            }

            const std::uint32_t indirectCommandOffset =
                static_cast<std::uint32_t>(m_IndirectCommandScratch.size() * sizeof(RHI::RHIDrawIndexedIndirectCommand));
            const std::uint32_t primitiveScratchOffset = static_cast<std::uint32_t>(m_IndirectPrimitiveScratch.size());
            bool validRun = true;
            for (std::uint32_t i = 0; i < runCommandCount; ++i)
            {
                const RenderCommand &command = commands[commandIndex + i];
                MeshGPUPrimitive *primitive = context.meshCache->GetOrCreate(context.device, context.assetManager, command, context.stats);
                if (primitive == nullptr || primitive->indexCount == 0)
                {
                    validRun = false;
                    break;
                }

                RHI::RHIDrawIndexedIndirectCommand indirectCommand{};
                indirectCommand.indexCount = primitive->indexCount;
                indirectCommand.instanceCount = command.instanceCount;
                indirectCommand.firstIndex = primitive->firstIndex;
                indirectCommand.vertexOffset = primitive->vertexOffset;
                indirectCommand.firstInstance = command.firstInstanceIndex;
                m_IndirectCommandScratch.push_back(indirectCommand);
                m_IndirectPrimitiveScratch.push_back(primitive);
            }

            if (!validRun)
            {
                const std::uint32_t commandScratchSize =
                    indirectCommandOffset / static_cast<std::uint32_t>(sizeof(RHI::RHIDrawIndexedIndirectCommand));
                m_IndirectCommandScratch.resize(commandScratchSize);
                m_IndirectPrimitiveScratch.resize(primitiveScratchOffset);
                commandIndex += runCommandCount;
                continue;
            }

            m_IndirectRuns.push_back(IndirectRun{commandIndex, runCommandCount, indirectCommandOffset, primitiveScratchOffset, nullptr});
            commandIndex += runCommandCount;
        }

        RHI::RHIBuffer *indirectBuffer = UploadIndirectCommands(context);
        if (indirectBuffer == nullptr)
        {
            m_IndirectRuns.clear();
            m_IndirectCommandScratch.clear();
            return;
        }

        for (IndirectRun &run : m_IndirectRuns)
        {
            run.indirectBuffer = indirectBuffer;
        }
    }

    RHI::RHIBuffer *ForwardOpaquePass::UploadIndirectCommands(const ForwardPassContext &context)
    {
        if (context.device == nullptr || m_IndirectCommandScratch.empty())
        {
            return nullptr;
        }

        if (m_IndirectBufferCursor >= m_IndirectCommandBuffers.size())
        {
            m_IndirectCommandBuffers.push_back(nullptr);
        }

        const std::uint32_t requiredBytes =
            static_cast<std::uint32_t>(m_IndirectCommandScratch.size() * sizeof(RHI::RHIDrawIndexedIndirectCommand));
        std::unique_ptr<RHI::RHIBuffer> &buffer = m_IndirectCommandBuffers[m_IndirectBufferCursor++];
        if (buffer == nullptr || buffer->GetSize() < requiredBytes)
        {
            std::uint32_t commandCapacity = 32u;
            const std::uint32_t requiredCommands = static_cast<std::uint32_t>(m_IndirectCommandScratch.size());
            while (commandCapacity < requiredCommands)
            {
                commandCapacity *= 2u;
            }

            RHI::RHIBufferDesc desc{};
            desc.size = commandCapacity * static_cast<std::uint32_t>(sizeof(RHI::RHIDrawIndexedIndirectCommand));
            desc.usage = RHI::BufferUsage::Indirect;
            desc.dynamic = true;
            buffer = context.device->CreateBuffer(desc);
        }

        if (buffer == nullptr)
        {
            return nullptr;
        }

        buffer->UploadData(m_IndirectCommandScratch.data(), requiredBytes);
        if (context.stats != nullptr)
        {
            context.stats->bufferUploadBytes += requiredBytes;
            ++context.stats->bufferUploadChunks;
        }
        return buffer.get();
    }

    void ForwardOpaquePass::SubmitDirectCommand(const ForwardPassContext &context, const RenderCommand &command, bool transparent)
    {
        MeshGPUPrimitive *primitive = context.meshCache != nullptr
                                          ? context.meshCache->GetOrCreate(context.device, context.assetManager, command, context.stats)
                                          : nullptr;
        if (primitive == nullptr || primitive->indexCount == 0 || !BindMaterial(context, command))
        {
            return;
        }

        context.commandList->SetRenderPrimitive(primitive->AsRHIRenderPrimitive());
        context.commandList->DrawIndexed(primitive->indexCount, command.instanceCount, primitive->firstIndex, primitive->vertexOffset, command.firstInstanceIndex);
        RecordSubmittedCommand(context, *primitive, command.instanceCount, transparent);
        if (!m_LoggedFirstDraw)
        {
            PHYSARA_CORE_INFO("Forward draw submitted '{}': indices={}, objectIndex={}, instances={}.",
                              MeshGPUCache::BuildMeshPrimitiveDebugName(command),
                              primitive->indexCount,
                              command.firstObjectIndex,
                              command.instanceCount);
            m_LoggedFirstDraw = true;
        }
    }

    void ForwardOpaquePass::SubmitIndirectRun(
        const ForwardPassContext &context,
        std::span<const RenderCommand> commands,
        const IndirectRun &run,
        bool transparent)
    {
        if (run.indirectBuffer == nullptr || run.commandIndex >= commands.size() ||
            run.primitiveScratchOffset + run.commandCount > m_IndirectPrimitiveScratch.size())
        {
            return;
        }

        MeshGPUPrimitive *firstPrimitive = m_IndirectPrimitiveScratch[run.primitiveScratchOffset];
        if (firstPrimitive == nullptr)
        {
            return;
        }

        const RenderCommand &firstCommand = commands[run.commandIndex];
        if (!BindMaterial(context, firstCommand))
        {
            return;
        }

        context.commandList->SetRenderPrimitive(firstPrimitive->AsRHIRenderPrimitive());
        context.commandList->DrawIndexedIndirect(
            run.indirectBuffer,
            run.commandCount,
            static_cast<std::uint32_t>(sizeof(RHI::RHIDrawIndexedIndirectCommand)),
            run.indirectCommandOffset);

        std::uint64_t submittedInstances = 0;
        for (std::uint32_t i = 0; i < run.commandCount; ++i)
        {
            const RenderCommand &command = commands[run.commandIndex + i];
            MeshGPUPrimitive *primitive = m_IndirectPrimitiveScratch[run.primitiveScratchOffset + i];
            if (primitive == nullptr)
            {
                continue;
            }
            submittedInstances += command.instanceCount;
            RecordSubmittedCommand(context, *primitive, command.instanceCount, transparent);
        }
        if (!m_LoggedFirstDraw)
        {
            PHYSARA_CORE_INFO("Forward MDI submitted '{}': indices={}, commands={}, instances={}.",
                              MeshGPUCache::BuildMeshPrimitiveDebugName(firstCommand),
                              firstPrimitive->indexCount,
                              run.commandCount,
                              submittedInstances);
            m_LoggedFirstDraw = true;
        }
    }

    void ForwardOpaquePass::RecordSubmittedCommand(
        const ForwardPassContext &context,
        const MeshGPUPrimitive &primitive,
        std::uint32_t instanceCount,
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
        context.stats->instances += instanceCount;
        context.stats->triangles += static_cast<std::uint64_t>(primitive.indexCount / 3u) * instanceCount;
    }

    void ForwardOpaquePass::ResetTextureBindings()
    {
        m_BoundMaterialInstanceId = InvalidMaterialInstanceId;
    }

}