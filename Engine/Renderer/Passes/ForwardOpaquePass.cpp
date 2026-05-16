#include "ForwardOpaquePass.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
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

        struct MaterialGPUData
        {
            glm::vec4 baseColor{1.f};
            glm::vec4 emissiveColorLuminance{0.f, 0.f, 0.f, 0.f};
            glm::vec4 metallicRoughnessReflectanceAO{0.f, 0.5f, 0.5f, 1.f};
            glm::vec4 alphaNormalFlags{0.5f, 1.f, 0.f, 0.f};
        };

        struct LightBufferHeader
        {
            std::uint32_t lightCount{0};
            std::uint32_t padding0{0};
            std::uint32_t padding1{0};
            std::uint32_t padding2{0};
        };

        constexpr std::uint32_t VertexStride = sizeof(float) * 12u;

        RHI::RHIBufferDesc DynamicBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = std::max(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        MaterialGPUData BuildDefaultMaterial()
        {
            MaterialGPUData material{};
            material.alphaNormalFlags.z = 0.f;
            material.alphaNormalFlags.w = 0.f;
            return material;
        }
    }

    void ForwardOpaquePass::Execute(const ForwardPassContext &context)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.renderProxy == nullptr)
        {
            return;
        }

        EnsureFrameBuffers(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);

        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, std::vector<glm::vec4>{context.clearColor});

        if (pipeline != nullptr)
        {
            context.commandList->SetPipelineState(pipeline);
            context.commandList->SetUniformBuffer(ForwardOpaquePassDetail::CameraBinding, m_CameraBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::ObjectBinding, m_ObjectBuffer.get());
            context.commandList->SetUniformBuffer(ForwardOpaquePassDetail::MaterialBinding, m_MaterialBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::LightBinding, m_LightBuffer.get());
        }

        // GLTF geometry upload is not implemented yet. Draw calls are intentionally skipped until Mesh owns GPU buffers.
        context.commandList->EndRenderPass();
    }

    void ForwardOpaquePass::EnsureFrameBuffers(const ForwardPassContext &context)
    {
        const FrameData &frameData = *context.frameData;

        if (m_CameraBuffer == nullptr)
        {
            m_CameraBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(sizeof(CameraData), RHI::BufferUsage::Uniform));
        }

        const std::uint32_t objectBufferSize = static_cast<std::uint32_t>(std::max<std::size_t>(frameData.objects.size(), 1u) * sizeof(ObjectData));
        if (m_ObjectBuffer == nullptr || m_ObjectBuffer->GetSize() < objectBufferSize)
        {
            m_ObjectBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(objectBufferSize, RHI::BufferUsage::Storage));
        }

        const std::uint32_t lightBufferSize =
            static_cast<std::uint32_t>(sizeof(ForwardOpaquePassDetail::LightBufferHeader) +
                                       std::max<std::size_t>(frameData.lights.size(), 1u) * sizeof(LightData));
        if (m_LightBuffer == nullptr || m_LightBuffer->GetSize() < lightBufferSize)
        {
            m_LightBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(lightBufferSize, RHI::BufferUsage::Storage));
        }

        if (m_MaterialBuffer == nullptr)
        {
            m_MaterialBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(sizeof(ForwardOpaquePassDetail::MaterialGPUData), RHI::BufferUsage::Uniform));
        }

        m_CameraBuffer->UploadData(&frameData.camera, sizeof(CameraData));
        if (!frameData.objects.empty())
        {
            m_ObjectBuffer->UploadData(frameData.objects.data(), objectBufferSize);
        }

        ForwardOpaquePassDetail::LightBufferHeader lightHeader{};
        lightHeader.lightCount = static_cast<std::uint32_t>(frameData.lights.size());
        m_LightBuffer->UploadData(&lightHeader, sizeof(lightHeader));
        if (!frameData.lights.empty())
        {
            m_LightBuffer->UploadData(
                frameData.lights.data(),
                static_cast<std::uint32_t>(frameData.lights.size() * sizeof(LightData)),
                sizeof(lightHeader));
        }

        const ForwardOpaquePassDetail::MaterialGPUData material = ForwardOpaquePassDetail::BuildDefaultMaterial();
        m_MaterialBuffer->UploadData(&material, sizeof(material));
    }

    RHI::RHIPipelineState *ForwardOpaquePass::GetPipeline(const ForwardPassContext &context)
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
        pipelineDesc.vertexAttributes.push_back({0u, 0u, RHI::VertexFormat::RGB32F, 0u});
        pipelineDesc.vertexAttributes.push_back({1u, 0u, RHI::VertexFormat::RGB32F, sizeof(float) * 3u});
        pipelineDesc.vertexAttributes.push_back({2u, 0u, RHI::VertexFormat::RGBA32F, sizeof(float) * 6u});
        pipelineDesc.vertexAttributes.push_back({3u, 0u, RHI::VertexFormat::RG16F, sizeof(float) * 10u});
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::Back;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = true;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        pipelineDesc.blendStates.push_back({});
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }
}