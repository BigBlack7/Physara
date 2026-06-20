#include "SkyboxPass.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/GPUContracts.hpp>
#include <Engine/Renderer/GPUScene.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Resource/Loaders/TextureLoader.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::Engine
{
    namespace SkyboxPassDetail
    {
        constexpr std::uint32_t FrameUniformsBinding = Binding(GPUBufferBinding::FrameUniforms);
        constexpr std::uint32_t SettingsBinding = Binding(GPUBufferBinding::SkyboxSettings);
        constexpr std::uint32_t SkyboxTextureBinding = Binding(GPUTextureBinding::Skybox);

        struct SettingsGPUData
        {
            glm::vec4 params{0.f};
        };

        Texture BuildPlaceholderPanorama(std::uint32_t width, std::uint32_t height)
        {
            Texture texture{};
            texture.width = width;
            texture.height = height;
            texture.channels = 4u;
            texture.sourceFormat = TextureSourceFormat::EXR;
            texture.rgba32fPixels.resize(static_cast<std::size_t>(width) * height * 4u);

            for (std::uint32_t y = 0u; y < height; ++y)
            {
                const float fy = static_cast<float>(y) / static_cast<float>(height - 1u);
                const glm::vec3 top(0.38f, 0.52f, 0.72f);
                const glm::vec3 horizon(0.74f, 0.82f, 0.78f);
                const glm::vec3 color = glm::mix(top, horizon, fy);
                for (std::uint32_t x = 0u; x < width; ++x)
                {
                    const std::size_t base = (static_cast<std::size_t>(y) * width + x) * 4u;
                    texture.rgba32fPixels[base + 0u] = color.r;
                    texture.rgba32fPixels[base + 1u] = color.g;
                    texture.rgba32fPixels[base + 2u] = color.b;
                    texture.rgba32fPixels[base + 3u] = 1.f;
                }
            }

            return texture;
        }

        glm::vec3 AverageColor(const std::vector<float> &pixels)
        {
            if (pixels.empty())
            {
                return {};
            }

            glm::vec3 sum(0.f);
            const std::size_t pixelCount = pixels.size() / 4u;
            for (std::size_t i = 0u; i < pixelCount; ++i)
            {
                sum += glm::vec3(pixels[i * 4u + 0u], pixels[i * 4u + 1u], pixels[i * 4u + 2u]);
            }
            return sum / static_cast<float>(pixelCount);
        }
    }

    void SkyboxPass::Execute(const SkyboxPassContext &context)
    {
        if (!context.enabled || context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.device == nullptr || context.frameUploadAllocator == nullptr || context.gpuScene == nullptr)
        {
            return;
        }

        EnsureResources(context);
        EnsureSkyboxTexture(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        if (pipeline == nullptr || m_SkyboxTexture == nullptr)
        {
            return;
        }

        const SkyboxPassDetail::SettingsGPUData settingsData{glm::vec4(context.exposureCompensation, 0.f, 0.f, 0.f)};
        const FrameUploadAllocation &frameUniformAllocation = context.gpuScene->GetFrameUniformBuffer();
        const FrameUploadAllocation settingsAllocation = context.frameUploadAllocator->Upload(*context.device, settingsData, context.stats);
        if (!frameUniformAllocation.IsValid() || !settingsAllocation.IsValid())
        {
            return;
        }
        context.frameUploadAllocator->Flush(context.stats);

        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, std::span<const glm::vec4>{});
        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(SkyboxPassDetail::FrameUniformsBinding, frameUniformAllocation.buffer, frameUniformAllocation.offset, frameUniformAllocation.size);
        context.commandList->SetUniformBuffer(SkyboxPassDetail::SettingsBinding, settingsAllocation.buffer, settingsAllocation.offset, settingsAllocation.size);
        context.commandList->SetTexture(SkyboxPassDetail::SkyboxTextureBinding, m_SkyboxTexture.get(), m_Sampler.get());
        constexpr std::uint32_t skySphereVertexCount = 64u * 32u * 6u;
        context.commandList->Draw(skySphereVertexCount, 1u, 0u, 0u);
        if (context.stats != nullptr)
        {
            ++context.stats->drawCalls;
            ++context.stats->skyboxDrawCalls;
            ++context.stats->instances;
            context.stats->triangles += skySphereVertexCount / 3u;
        }
        context.commandList->EndRenderPass();
    }

    void SkyboxPass::InvalidateEnvironment()
    {
        m_LoadedEnvironmentPath.clear();
        m_SkyboxTexture.reset();
    }

    void SkyboxPass::EnsureResources(const SkyboxPassContext &context)
    {
        if (m_Sampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Nearest;
            desc.wrapU = RHI::WrapMode::Repeat;
            desc.wrapV = RHI::WrapMode::ClampToEdge;
            desc.wrapW = RHI::WrapMode::ClampToEdge;
            desc.anisotropy = 1.f;
            m_Sampler = context.device->CreateSampler(desc);
        }
    }

    void SkyboxPass::EnsureSkyboxTexture(const SkyboxPassContext &context)
    {
        const std::filesystem::path requestedPath = context.environmentPath.empty()
                                                       ? std::filesystem::path{}
                                                       : context.environmentPath.lexically_normal();
        if (m_SkyboxTexture != nullptr && m_LoadedEnvironmentPath == requestedPath)
        {
            return;
        }

        if (!requestedPath.empty())
        {
            std::shared_ptr<Texture> texture = TextureLoader::LoadRGBA32F(requestedPath);
            if (texture != nullptr && texture->IsLoaded() && !texture->rgba32fPixels.empty())
            {
                const glm::vec3 averageColor = SkyboxPassDetail::AverageColor(texture->rgba32fPixels);
                if (UploadPanorama(context, *texture))
                {
                    m_LoadedEnvironmentPath = requestedPath;
                    PHYSARA_CORE_INFO("Skybox environment loaded '{}': panorama={}x{}, format=RGBA32F, avg=({}, {}, {}).",
                                      requestedPath.string(),
                                      texture->width,
                                      texture->height,
                                      averageColor.r,
                                      averageColor.g,
                                      averageColor.b);
                    return;
                }
            }

            PHYSARA_CORE_WARN("Skybox environment '{}' could not be loaded; using placeholder.", requestedPath.string());
        }

        if (UploadPanorama(context, SkyboxPassDetail::BuildPlaceholderPanorama(32u, 16u)))
        {
            m_LoadedEnvironmentPath = requestedPath;
            if (!m_LoggedPlaceholder)
            {
                PHYSARA_CORE_INFO("Skybox using placeholder cubemap.");
                m_LoggedPlaceholder = true;
            }
        }
    }

    bool SkyboxPass::UploadPanorama(const SkyboxPassContext &context, const Texture &panorama)
    {
        RHI::RHITextureDesc desc{};
        desc.width = panorama.width;
        desc.height = panorama.height;
        desc.mipLevels = 1u;
        desc.arrayLayers = 1u;
        desc.format = RHI::TextureFormat::RGBA32F;
        desc.dimension = RHI::TextureDimension::Tex2D;
        desc.usage = RHI::TextureUsage::Sampled;
        desc.initialData = panorama.rgba32fPixels.data();

        m_SkyboxTexture = context.device->CreateTexture(desc);
        if (m_SkyboxTexture == nullptr)
        {
            PHYSARA_CORE_ERROR("Failed to create skybox panorama texture.");
            return false;
        }

        if (context.stats != nullptr)
        {
            ++context.stats->textureUploads;
            context.stats->textureUploadBytes += static_cast<std::uint64_t>(panorama.rgba32fPixels.size() * sizeof(float));
        }
        return true;
    }

    RHI::RHIPipelineState *SkyboxPass::GetPipeline(const SkyboxPassContext &context)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "Skybox";
        shaderDesc.vertexPath = "Shaders/Passes/Skybox/Skybox.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/Skybox/Skybox.frag";

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