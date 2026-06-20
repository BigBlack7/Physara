#include "GBufferPass.hpp"

#include <array>
#include <cstddef>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::Engine
{
    namespace GBufferPassDetail
    {
        constexpr std::uint32_t FrameUniformsBinding = Binding(GPUBufferBinding::FrameUniforms);
        constexpr std::uint32_t ObjectBinding = Binding(GPUBufferBinding::Objects);
        constexpr std::uint32_t MaterialBinding = Binding(GPUBufferBinding::Materials);
        constexpr std::uint32_t InstanceObjectIndexBinding = Binding(GPUBufferBinding::InstanceIndices);
        constexpr std::uint32_t MaterialTextureIndexBinding = Binding(GPUBufferBinding::MaterialTextureIndices);
        constexpr std::uint32_t BindlessTextureHandleBinding = Binding(GPUBufferBinding::BindlessTextureHandles);
        constexpr std::uint32_t PerMaterialResourceSet = Binding(GPUResourceSetIndex::PerMaterial);
        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);
    }

    void GBufferPass::Execute(const GBufferPassContext &context)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.gpuScene == nullptr || context.renderProxy == nullptr ||
            context.materialTextureCache == nullptr)
        {
            return;
        }

        RHI::RHIPipelineState *singleSidedPipeline = GetPipeline(context, RHI::CullMode::Back);
        RHI::RHIPipelineState *doubleSidedPipeline = GetPipeline(context, RHI::CullMode::None);
        const FrameUploadAllocation &frameUniforms = context.gpuScene->GetFrameUniformBuffer();
        const FrameUploadAllocation &objects = context.gpuScene->GetObjectBuffer();
        const FrameUploadAllocation &instances = context.gpuScene->GetForwardInstanceObjectIndexBuffer();
        if (singleSidedPipeline == nullptr || doubleSidedPipeline == nullptr ||
            !frameUniforms.IsValid() || !objects.IsValid() || !instances.IsValid() ||
            context.gpuScene->GetMaterialBuffer() == nullptr)
        {
            return;
        }

        const std::array<glm::vec4, 4> clearColors{
            glm::vec4(0.f),
            glm::vec4(0.5f, 0.5f, 0.f, 0.f),
            glm::vec4(1.f, 0.f, 0.5f, 0.f),
            glm::vec4(0.f)};
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, clearColors);
        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->SetUniformBuffer(
            GBufferPassDetail::FrameUniformsBinding,
            frameUniforms.buffer,
            frameUniforms.offset,
            frameUniforms.size);
        context.commandList->SetStorageBuffer(
            GBufferPassDetail::ObjectBinding,
            objects.buffer,
            objects.offset,
            objects.size);
        context.commandList->SetStorageBuffer(GBufferPassDetail::MaterialBinding, context.gpuScene->GetMaterialBuffer());
        context.commandList->SetStorageBuffer(
            GBufferPassDetail::InstanceObjectIndexBinding,
            instances.buffer,
            instances.offset,
            instances.size);
        if (context.materialTextureCache->HasBindlessTables())
        {
            context.commandList->SetStorageBuffer(
                GBufferPassDetail::MaterialTextureIndexBinding,
                context.materialTextureCache->GetMaterialTextureIndexBuffer());
            context.commandList->SetStorageBuffer(
                GBufferPassDetail::BindlessTextureHandleBinding,
                context.materialTextureCache->GetBindlessTextureHandleBuffer());
        }

        m_CommandExecutor.BeginFrame();
        m_BoundMaterialTextureSetId = 0u;
        const RenderCommandBuckets &commands = context.renderProxy->GetCommands();
        context.commandList->SetPipelineState(singleSidedPipeline);
        DrawCommands(context, commands.opaque.singleSided);
        context.commandList->SetPipelineState(doubleSidedPipeline);
        DrawCommands(context, commands.opaque.doubleSided);
        context.commandList->EndRenderPass();
    }

    void GBufferPass::Reset()
    {
        m_CommandExecutor.Reset();
        m_BoundMaterialTextureSetId = 0u;
    }

    RHI::RHIPipelineState *GBufferPass::GetPipeline(const GBufferPassContext &context, RHI::CullMode cullMode)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "GBuffer";
        shaderDesc.vertexPath = "Shaders/Passes/Deferred/GBuffer.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/Deferred/GBuffer.frag";
        if (context.materialTextureCache->HasBindlessTables())
        {
            shaderDesc.defines.push_back({"PHYSARA_BINDLESS_MATERIAL_TEXTURES", "1"});
        }
        ShaderVariant *variant = context.shaderLibrary->GetVariant(shaderDesc);
        if (variant == nullptr || !variant->IsValid())
        {
            return nullptr;
        }

        RHI::RHIPipelineStateDesc pipelineDesc{};
        pipelineDesc.vertexShader = variant->vertexShader.get();
        pipelineDesc.fragmentShader = variant->fragmentShader.get();
        pipelineDesc.renderPassDesc = context.renderPassDesc;
        pipelineDesc.vertexBindings.push_back({0u, GBufferPassDetail::VertexStride, 0u});
        pipelineDesc.vertexAttributes.push_back({0u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, position))});
        pipelineDesc.vertexAttributes.push_back({1u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, normal))});
        pipelineDesc.vertexAttributes.push_back({2u, 0u, RHI::VertexFormat::RGBA32F, static_cast<std::uint32_t>(offsetof(MeshVertex, tangent))});
        pipelineDesc.vertexAttributes.push_back({3u, 0u, RHI::VertexFormat::RG32F, static_cast<std::uint32_t>(offsetof(MeshVertex, texCoord0))});
        pipelineDesc.vertexAttributes.push_back({4u, 0u, RHI::VertexFormat::RG32F, static_cast<std::uint32_t>(offsetof(MeshVertex, texCoord1))});
        pipelineDesc.rasterizerState.cullMode = cullMode;
        pipelineDesc.rasterizerState.polygonMode = context.wireframe ? RHI::PolygonMode::Line : RHI::PolygonMode::Fill;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = true;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        pipelineDesc.blendStates.resize(4u);
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    bool GBufferPass::BindMaterial(const GBufferPassContext &context, const RenderCommand &command)
    {
        if (command.firstObjectIndex >= context.frameData->objects.size())
        {
            return false;
        }
        const std::uint32_t materialIndex = context.frameData->objects[command.firstObjectIndex].materialIndex;
        const MaterialResourceSet *resourceSet = context.materialTextureCache->GetResourceSet(materialIndex);
        if (resourceSet == nullptr)
        {
            return false;
        }
        if (context.materialTextureCache->HasBindlessTables() || m_BoundMaterialTextureSetId == resourceSet->textureSetId)
        {
            return true;
        }
        context.commandList->SetResourceSet(GBufferPassDetail::PerMaterialResourceSet, resourceSet->AsRHIResourceSet());
        m_BoundMaterialTextureSetId = resourceSet->textureSetId;
        return true;
    }

    bool GBufferPass::CanMerge(
        const GBufferPassContext &context,
        const RenderCommand &lhs,
        const RenderCommand &rhs) const
    {
        if (lhs.firstObjectIndex >= context.frameData->objects.size() ||
            rhs.firstObjectIndex >= context.frameData->objects.size())
        {
            return false;
        }
        const MaterialResourceSet *lhsSet = context.materialTextureCache->GetResourceSet(
            context.frameData->objects[lhs.firstObjectIndex].materialIndex);
        const MaterialResourceSet *rhsSet = context.materialTextureCache->GetResourceSet(
            context.frameData->objects[rhs.firstObjectIndex].materialIndex);
        if (lhsSet == nullptr || rhsSet == nullptr || lhs.doubleSided != rhs.doubleSided)
        {
            return false;
        }
        return context.materialTextureCache->HasBindlessTables()
                   ? lhs.meshKey == rhs.meshKey
                   : lhsSet->textureSetId == rhsSet->textureSetId && lhs.meshKey == rhs.meshKey;
    }

    void GBufferPass::DrawCommands(const GBufferPassContext &context, std::span<const RenderCommand> commands)
    {
        CommandSubmitContext submitContext{this, &context};
        RenderCommandExecutorContext executorContext{};
        executorContext.device = context.device;
        executorContext.commandList = context.commandList;
        executorContext.meshCache = context.meshCache;
        executorContext.assetManager = context.assetManager;
        executorContext.stats = context.stats;
        RenderCommandSubmitCallbacks callbacks{};
        callbacks.userData = &submitContext;
        callbacks.canMergeIndirectRun = &GBufferPass::CanMergeSubmittedCommands;
        callbacks.bindCommand = &GBufferPass::BindSubmittedCommand;
        callbacks.recordCommand = &GBufferPass::RecordSubmittedCommand;
        m_CommandExecutor.Submit(executorContext, commands, callbacks);
    }

    bool GBufferPass::BindSubmittedCommand(void *userData, const RenderCommand &command)
    {
        auto *submit = static_cast<CommandSubmitContext *>(userData);
        return submit != nullptr && submit->pass != nullptr && submit->context != nullptr &&
               submit->pass->BindMaterial(*submit->context, command);
    }

    bool GBufferPass::CanMergeSubmittedCommands(void *userData, const RenderCommand &lhs, const RenderCommand &rhs)
    {
        auto *submit = static_cast<CommandSubmitContext *>(userData);
        return submit != nullptr && submit->pass != nullptr && submit->context != nullptr &&
               submit->pass->CanMerge(*submit->context, lhs, rhs);
    }

    void GBufferPass::RecordSubmittedCommand(
        void *userData,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode)
    {
        auto *submit = static_cast<CommandSubmitContext *>(userData);
        if (submit == nullptr || submit->context == nullptr || submit->context->stats == nullptr)
        {
            return;
        }
        ++submit->context->stats->drawCalls;
        ++submit->context->stats->deferredGBufferDrawCalls;
        submit->context->stats->instances += command.instanceCount;
        submit->context->stats->triangles += static_cast<std::uint64_t>(primitive.indexCount / 3u) * command.instanceCount;
    }
}