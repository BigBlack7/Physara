#include "SkyboxPass.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Resource/Loaders/TextureLoader.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::Engine
{
    namespace SkyboxPassDetail
    {
        constexpr std::uint32_t CameraBinding = 0u;
        constexpr std::uint32_t SettingsBinding = 4u;
        constexpr std::uint32_t SkyboxTextureBinding = 5u;
        constexpr float Pi = 3.14159265358979323846f;

        struct SettingsGPUData
        {
            glm::vec4 params{0.f};
        };

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        template <typename T>
        constexpr T MinValue(T lhs, T rhs)
        {
            return lhs < rhs ? lhs : rhs;
        }

        RHI::RHIBufferDesc DynamicBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = MaxValue(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        glm::vec3 FaceDirection(std::uint32_t face, float u, float v)
        {
            switch (face)
            {
            case 0u:
                return glm::normalize(glm::vec3(1.f, -v, -u));
            case 1u:
                return glm::normalize(glm::vec3(-1.f, -v, u));
            case 2u:
                return glm::normalize(glm::vec3(u, 1.f, v));
            case 3u:
                return glm::normalize(glm::vec3(u, -1.f, -v));
            case 4u:
                return glm::normalize(glm::vec3(u, -v, 1.f));
            case 5u:
            default:
                return glm::normalize(glm::vec3(-u, -v, -1.f));
            }
        }

        glm::vec4 SampleEquirectangular(const Texture &texture, const glm::vec3 &direction)
        {
            const float longitude = std::atan2(direction.z, direction.x);
            const float latitude = std::acos(MinValue(MaxValue(direction.y, -1.f), 1.f));
            float u = longitude / (2.f * Pi) + 0.5f;
            const float v = latitude / Pi;

            u = u - std::floor(u);
            const float x = u * static_cast<float>(texture.width - 1u);
            const float y = v * static_cast<float>(texture.height - 1u);
            const std::uint32_t x0 = static_cast<std::uint32_t>(std::floor(x)) % texture.width;
            const std::uint32_t y0 = MinValue(static_cast<std::uint32_t>(std::floor(y)), texture.height - 1u);
            const std::uint32_t x1 = (x0 + 1u) % texture.width;
            const std::uint32_t y1 = MinValue(y0 + 1u, texture.height - 1u);
            const float tx = x - std::floor(x);
            const float ty = y - std::floor(y);

            const auto read = [&texture](std::uint32_t px, std::uint32_t py)
            {
                const std::size_t base = (static_cast<std::size_t>(py) * texture.width + px) * 4u;
                return glm::vec4(
                    texture.rgba32fPixels[base + 0u],
                    texture.rgba32fPixels[base + 1u],
                    texture.rgba32fPixels[base + 2u],
                    texture.rgba32fPixels[base + 3u]);
            };

            const glm::vec4 a = read(x0, y0);
            const glm::vec4 b = read(x1, y0);
            const glm::vec4 c = read(x0, y1);
            const glm::vec4 d = read(x1, y1);
            return glm::mix(glm::mix(a, b, tx), glm::mix(c, d, tx), ty);
        }

        std::vector<float> BuildPlaceholderCubemap(std::uint32_t faceSize)
        {
            std::vector<float> pixels(static_cast<std::size_t>(faceSize) * faceSize * 6u * 4u);
            for (std::uint32_t face = 0u; face < 6u; ++face)
            {
                for (std::uint32_t y = 0u; y < faceSize; ++y)
                {
                    for (std::uint32_t x = 0u; x < faceSize; ++x)
                    {
                        const float fy = static_cast<float>(y) / static_cast<float>(faceSize - 1u);
                        const glm::vec3 top(0.38f, 0.52f, 0.72f);
                        const glm::vec3 horizon(0.74f, 0.82f, 0.78f);
                        const glm::vec3 color = glm::mix(top, horizon, fy);
                        const std::size_t base = ((static_cast<std::size_t>(face) * faceSize + y) * faceSize + x) * 4u;
                        pixels[base + 0u] = color.r;
                        pixels[base + 1u] = color.g;
                        pixels[base + 2u] = color.b;
                        pixels[base + 3u] = 1.f;
                    }
                }
            }
            return pixels;
        }

        std::vector<float> BuildCubemapFromEquirectangular(const Texture &texture, std::uint32_t faceSize)
        {
            std::vector<float> pixels(static_cast<std::size_t>(faceSize) * faceSize * 6u * 4u);
            for (std::uint32_t face = 0u; face < 6u; ++face)
            {
                for (std::uint32_t y = 0u; y < faceSize; ++y)
                {
                    for (std::uint32_t x = 0u; x < faceSize; ++x)
                    {
                        const float u = (2.f * (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize)) - 1.f;
                        const float v = (2.f * (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize)) - 1.f;
                        const glm::vec4 sample = SampleEquirectangular(texture, FaceDirection(face, u, v));
                        const std::size_t base = ((static_cast<std::size_t>(face) * faceSize + y) * faceSize + x) * 4u;
                        pixels[base + 0u] = sample.r;
                        pixels[base + 1u] = sample.g;
                        pixels[base + 2u] = sample.b;
                        pixels[base + 3u] = sample.a;
                    }
                }
            }
            return pixels;
        }

        std::uint32_t ChooseFaceSize(const Texture &texture)
        {
            if (texture.width == 0u || texture.height == 0u)
            {
                return 16u;
            }
            return MinValue<std::uint32_t>(1024u, MaxValue<std::uint32_t>(64u, texture.height / 2u));
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
            context.frameData == nullptr || context.device == nullptr)
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

        m_CameraBuffer->UploadData(&context.frameData->camera, sizeof(CameraData));
        const SkyboxPassDetail::SettingsGPUData settingsData{glm::vec4(context.exposureCompensation, 0.f, 0.f, 0.f)};
        m_SettingsBuffer->UploadData(&settingsData, sizeof(settingsData));

        context.commandList->SetViewport(
            0.f,
            0.f,
            static_cast<float>(context.frameData->view.viewport.width),
            static_cast<float>(context.frameData->view.viewport.height));
        context.commandList->SetScissor(0, 0, context.frameData->view.viewport.width, context.frameData->view.viewport.height);
        context.commandList->BeginRenderPass(context.framebuffer, *context.renderPassDesc, std::vector<glm::vec4>{});
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(SkyboxPassDetail::CameraBinding, m_CameraBuffer.get());
        context.commandList->SetUniformBuffer(SkyboxPassDetail::SettingsBinding, m_SettingsBuffer.get());
        context.commandList->SetTexture(SkyboxPassDetail::SkyboxTextureBinding, m_SkyboxTexture.get(), m_Sampler.get());
        context.commandList->Draw(36u, 1u, 0u, 0u);
        context.commandList->EndRenderPass();
    }

    void SkyboxPass::InvalidateEnvironment()
    {
        m_LoadedEnvironmentPath.clear();
        m_SkyboxTexture.reset();
    }

    void SkyboxPass::EnsureResources(const SkyboxPassContext &context)
    {
        if (m_CameraBuffer == nullptr)
        {
            m_CameraBuffer = context.device->CreateBuffer(
                SkyboxPassDetail::DynamicBufferDesc(sizeof(CameraData), RHI::BufferUsage::Uniform));
        }

        if (m_SettingsBuffer == nullptr)
        {
            m_SettingsBuffer = context.device->CreateBuffer(
                SkyboxPassDetail::DynamicBufferDesc(sizeof(SkyboxPassDetail::SettingsGPUData), RHI::BufferUsage::Uniform));
        }

        if (m_Sampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::ClampToEdge;
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
                const std::uint32_t faceSize = SkyboxPassDetail::ChooseFaceSize(*texture);
                const std::vector<float> cubemapPixels = SkyboxPassDetail::BuildCubemapFromEquirectangular(*texture, faceSize);
                const glm::vec3 averageColor = SkyboxPassDetail::AverageColor(cubemapPixels);
                UploadCubemap(context, faceSize, cubemapPixels);
                m_LoadedEnvironmentPath = requestedPath;
                PHYSARA_CORE_INFO("Skybox environment loaded '{}': panorama={}x{}, cubemap={}x{}, avg=({}, {}, {}).",
                                  requestedPath.string(),
                                  texture->width,
                                  texture->height,
                                  faceSize,
                                  faceSize,
                                  averageColor.r,
                                  averageColor.g,
                                  averageColor.b);
                return;
            }

            PHYSARA_CORE_WARN("Skybox environment '{}' could not be loaded; using placeholder.", requestedPath.string());
        }

        constexpr std::uint32_t placeholderSize = 16u;
        UploadCubemap(context, placeholderSize, SkyboxPassDetail::BuildPlaceholderCubemap(placeholderSize));
        m_LoadedEnvironmentPath = requestedPath;
        if (!m_LoggedPlaceholder)
        {
            PHYSARA_CORE_INFO("Skybox using placeholder cubemap.");
            m_LoggedPlaceholder = true;
        }
    }

    void SkyboxPass::UploadCubemap(const SkyboxPassContext &context, std::uint32_t faceSize, const std::vector<float> &pixels)
    {
        RHI::RHITextureDesc desc{};
        desc.width = faceSize;
        desc.height = faceSize;
        desc.mipLevels = 1u;
        desc.arrayLayers = 6u;
        desc.format = RHI::TextureFormat::RGBA16F;
        desc.dimension = RHI::TextureDimension::TexCube;
        desc.usage = RHI::TextureUsage::Sampled;

        m_SkyboxTexture = context.device->CreateTexture(desc);
        if (m_SkyboxTexture == nullptr)
        {
            PHYSARA_CORE_ERROR("Failed to create skybox cubemap.");
            return;
        }

        const std::size_t faceFloatCount = static_cast<std::size_t>(faceSize) * faceSize * 4u;
        for (std::uint32_t face = 0u; face < 6u; ++face)
        {
            m_SkyboxTexture->Upload(0u, face, pixels.data() + static_cast<std::size_t>(face) * faceFloatCount);
        }
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
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = false;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::LessEqual;
        pipelineDesc.blendStates.push_back({});
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }
}