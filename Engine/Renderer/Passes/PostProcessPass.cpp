#include "PostProcessPass.hpp"

#include <algorithm>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Renderer/PipelineStateCache.hpp>
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

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        struct SettingsGPUData
        {
            glm::vec4 bloomParams{1.f, 0.5f, 0.08f, 2.f};
            glm::vec4 flags{1.f, 1.f, 1.f, 0.f};
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
                settings.toneMappingEnabled ? 1.f : 0.f,
                settings.bloomEnabled ? 1.f : 0.f,
                settings.fxaaEnabled ? 1.f : 0.f,
                0.f);
            return data;
        }
    }

    void PostProcessPass::Execute(const PostProcessPassContext &context)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.sceneHDR == nullptr || context.device == nullptr)
        {
            return;
        }

        EnsureResources(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        if (pipeline == nullptr)
        {
            return;
        }

        const PostProcessPassDetail::SettingsGPUData settingsData = PostProcessPassDetail::BuildSettings(context.settings);
        m_CameraBuffer->UploadData(&context.frameData->camera, sizeof(CameraData));
        m_SettingsBuffer->UploadData(&settingsData, sizeof(settingsData));

        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, std::vector<glm::vec4>{glm::vec4(0.f, 0.f, 0.f, 1.f)});
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(PostProcessPassDetail::CameraBinding, m_CameraBuffer.get());
        context.commandList->SetUniformBuffer(PostProcessPassDetail::SettingsBinding, m_SettingsBuffer.get());
        context.commandList->SetTexture(PostProcessPassDetail::SceneColorBinding, context.sceneHDR, m_LinearClampSampler.get());
        context.commandList->Draw(3u, 1u, 0u, 0u);
        context.commandList->EndRenderPass();
    }

    void PostProcessPass::EnsureResources(const PostProcessPassContext &context)
    {
        if (m_CameraBuffer == nullptr)
        {
            m_CameraBuffer = context.device->CreateBuffer(
                PostProcessPassDetail::DynamicBufferDesc(sizeof(CameraData), RHI::BufferUsage::Uniform));
        }

        if (m_SettingsBuffer == nullptr)
        {
            m_SettingsBuffer = context.device->CreateBuffer(
                PostProcessPassDetail::DynamicBufferDesc(sizeof(PostProcessPassDetail::SettingsGPUData), RHI::BufferUsage::Uniform));
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
}