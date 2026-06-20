#include "WorldGridPass.hpp"

#include <algorithm>
#include <span>

#include <Engine/Renderer/GPUContracts.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::Engine
{
    namespace WorldGridPassDetail
    {
        constexpr std::uint32_t FrameUniformsBinding = Binding(GPUBufferBinding::FrameUniforms);
        constexpr std::uint32_t SettingsBinding = Binding(GPUBufferBinding::WorldGridSettings);

        struct SettingsGPUData
        {
            glm::vec4 spacingFade{1.f, 10.f, 50.f, 500.f};
            glm::vec4 minorColor{0.28f, 0.32f, 0.31f, 0.24f};
            glm::vec4 majorColor{0.42f, 0.47f, 0.45f, 0.42f};
            glm::vec4 xAxisColor{0.78f, 0.20f, 0.16f, 0.85f};
            glm::vec4 zAxisColor{0.18f, 0.38f, 0.82f, 0.85f};
        };

        SettingsGPUData BuildSettings(const WorldGridSettings &settings)
        {
            SettingsGPUData data{};
            const float minorSpacing = std::max(settings.minorSpacingMeters, 0.001f);
            const float fadeStart = std::max(settings.fadeStartMeters, minorSpacing);
            const float fadeEnd = std::max(settings.fadeEndMeters, fadeStart + minorSpacing);
            data.spacingFade = glm::vec4(
                minorSpacing,
                static_cast<float>(std::max(settings.majorLineInterval, 2u)),
                fadeStart,
                fadeEnd);
            return data;
        }
    }

    void WorldGridPass::Execute(const WorldGridPassContext &context)
    {
        if (!context.settings.enabled || context.device == nullptr || context.commandList == nullptr ||
            context.framebuffer == nullptr || context.renderPassDesc == nullptr || context.shaderLibrary == nullptr ||
            context.pipelineCache == nullptr || context.frameData == nullptr || context.frameUploadAllocator == nullptr ||
            context.gpuScene == nullptr)
        {
            return;
        }

        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        const FrameUploadAllocation &frameUniforms = context.gpuScene->GetFrameUniformBuffer();
        const WorldGridPassDetail::SettingsGPUData settingsData = WorldGridPassDetail::BuildSettings(context.settings);
        const FrameUploadAllocation settings =
            context.frameUploadAllocator->Upload(*context.device, settingsData, context.stats);
        if (pipeline == nullptr || !frameUniforms.IsValid() || !settings.IsValid())
        {
            return;
        }
        context.frameUploadAllocator->Flush(context.stats);

        context.commandList->BeginRenderPass(
            context.framebuffer,
            *context.renderPassDesc,
            std::span<const glm::vec4>{});
        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(
            0,
            0,
            context.frameData->view.viewport.width,
            context.frameData->view.viewport.height);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(
            WorldGridPassDetail::FrameUniformsBinding,
            frameUniforms.buffer,
            frameUniforms.offset,
            frameUniforms.size);
        context.commandList->SetUniformBuffer(
            WorldGridPassDetail::SettingsBinding,
            settings.buffer,
            settings.offset,
            settings.size);
        context.commandList->Draw(3u, 1u, 0u, 0u);
        if (context.stats != nullptr)
        {
            ++context.stats->drawCalls;
            ++context.stats->instances;
            ++context.stats->triangles;
        }
        context.commandList->EndRenderPass();
    }

    RHI::RHIPipelineState *WorldGridPass::GetPipeline(const WorldGridPassContext &context)
    {
        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "WorldGrid";
        shaderDesc.vertexPath = "Shaders/Passes/WorldGrid/WorldGrid.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/WorldGrid/WorldGrid.frag";
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
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = false;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::LessEqual;
        RHI::RHIBlendState blendState{};
        blendState.blendEnable = true;
        blendState.srcColor = RHI::BlendFactor::SrcAlpha;
        blendState.dstColor = RHI::BlendFactor::OneMinusSrcAlpha;
        blendState.srcAlpha = RHI::BlendFactor::One;
        blendState.dstAlpha = RHI::BlendFactor::OneMinusSrcAlpha;
        pipelineDesc.blendStates.push_back(blendState);
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }
}