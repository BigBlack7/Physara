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
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/Resource/Types/Texture.hpp>
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
        constexpr std::uint32_t CameraBinding = 0u;
        constexpr std::uint32_t ObjectBinding = 1u;
        constexpr std::uint32_t MaterialBinding = 2u;
        constexpr std::uint32_t LightBinding = 3u;
        constexpr std::uint32_t BaseColorTextureBinding = 0u;
        constexpr std::uint32_t MetallicRoughnessTextureBinding = 1u;
        constexpr std::uint32_t NormalTextureBinding = 2u;
        constexpr std::uint32_t OcclusionTextureBinding = 3u;
        constexpr std::uint32_t EmissiveTextureBinding = 4u;

        struct MaterialGPUData
        {
            glm::vec4 baseColor{1.f};
            glm::vec4 emissiveColorLuminance{0.f, 0.f, 0.f, 0.f};
            glm::vec4 metallicRoughnessReflectanceAO{0.f, 0.5f, 0.5f, 1.f};
            glm::vec4 alphaNormalFlags{0.5f, 1.f, 0.f, 0.f};
            glm::vec4 textureFlags{0.f, 0.f, 0.f, 0.f};
        };

        struct LightBufferHeader
        {
            std::uint32_t lightCount{0};
            std::uint32_t padding0{0};
            std::uint32_t padding1{0};
            std::uint32_t padding2{0};
        };

        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);

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

        float ShadingModelToShaderValue(ShadingModel model)
        {
            return model == ShadingModel::Unlit ? 1.f : 0.f;
        }

        float AlphaModeToShaderValue(AlphaMode mode)
        {
            switch (mode)
            {
            case AlphaMode::Mask:
                return 1.f;
            case AlphaMode::Blend:
                return 2.f;
            case AlphaMode::Opaque:
            default:
                return 0.f;
            }
        }

        MaterialGPUData BuildMaterial(const MaterialComponent &component)
        {
            MaterialComponent materialComponent = component;
            materialComponent.Sanitize();

            MaterialGPUData material{};
            material.baseColor = materialComponent.baseColor;
            material.emissiveColorLuminance = glm::vec4(materialComponent.emissiveColor, materialComponent.emissiveLuminance);
            material.metallicRoughnessReflectanceAO = glm::vec4(
                materialComponent.metallic,
                materialComponent.roughness,
                0.5f,
                materialComponent.ambientOcclusion);
            material.alphaNormalFlags = glm::vec4(
                materialComponent.alphaCutoff,
                materialComponent.normalScale,
                ShadingModelToShaderValue(materialComponent.shadingModel),
                AlphaModeToShaderValue(materialComponent.alphaMode));
            material.textureFlags = glm::vec4(
                materialComponent.baseColorTexture.IsBound() ? 1.f : 0.f,
                materialComponent.metallicRoughnessTexture.IsBound() ? 1.f : 0.f,
                materialComponent.normalTexture.IsBound() ? 1.f : 0.f,
                materialComponent.occlusionTexture.IsBound() ? 1.f : 0.f);
            return material;
        }

        RHI::RHITextureDesc TextureDesc(std::uint32_t width, std::uint32_t height, const void *pixels, std::uint32_t mipLevels = 1u)
        {
            RHI::RHITextureDesc desc{};
            desc.width = std::max(width, 1u);
            desc.height = std::max(height, 1u);
            desc.mipLevels = std::max(mipLevels, 1u);
            desc.format = RHI::TextureFormat::RGBA8;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = pixels;
            return desc;
        }

        std::uint32_t CalculateMipLevels(std::uint32_t width, std::uint32_t height)
        {
            std::uint32_t levels = 1u;
            std::uint32_t size = std::max(width, height);
            while (size > 1u)
            {
                size >>= 1u;
                ++levels;
            }
            return levels;
        }

        template <typename T>
        RHI::RHIBufferDesc StaticBufferDesc(const std::vector<T> &data, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = static_cast<std::uint32_t>(data.size() * sizeof(T));
            desc.usage = usage;
            desc.dynamic = false;
            desc.initialData = data.data();
            return desc;
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
        EnsureDefaultTextures(context);
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

            context.commandList->SetPipelineState(pipeline);
            context.commandList->SetUniformBuffer(ForwardOpaquePassDetail::CameraBinding, m_CameraBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::ObjectBinding, m_ObjectBuffer.get());
            context.commandList->SetUniformBuffer(ForwardOpaquePassDetail::MaterialBinding, m_MaterialBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::LightBinding, m_LightBuffer.get());
            DrawBucket(context, context.renderProxy->GetBuckets().opaque);
            DrawBucket(context, context.renderProxy->GetBuckets().unlit);
        }

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

    void ForwardOpaquePass::EnsureDefaultTextures(const ForwardPassContext &context)
    {
        if (context.device == nullptr)
        {
            return;
        }

        if (m_LinearRepeatSampler == nullptr)
        {
            RHI::RHISamplerDesc desc{};
            desc.minFilter = RHI::FilterMode::Linear;
            desc.magFilter = RHI::FilterMode::Linear;
            desc.mipFilter = RHI::FilterMode::Linear;
            desc.wrapU = RHI::WrapMode::Repeat;
            desc.wrapV = RHI::WrapMode::Repeat;
            desc.wrapW = RHI::WrapMode::Repeat;
            desc.anisotropy = static_cast<float>(std::max(context.device->GetMaxAnisotropy(), 1));
            m_LinearRepeatSampler = context.device->CreateSampler(desc);
        }

        if (m_FallbackWhiteTexture == nullptr)
        {
            const std::array<std::uint8_t, 4> white{255u, 255u, 255u, 255u};
            m_FallbackWhiteTexture = context.device->CreateTexture(ForwardOpaquePassDetail::TextureDesc(1u, 1u, white.data()));
        }

        if (m_FallbackNormalTexture == nullptr)
        {
            const std::array<std::uint8_t, 4> normal{128u, 128u, 255u, 255u};
            m_FallbackNormalTexture = context.device->CreateTexture(ForwardOpaquePassDetail::TextureDesc(1u, 1u, normal.data()));
        }
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
        pipelineDesc.vertexAttributes.push_back({0u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, position))});
        pipelineDesc.vertexAttributes.push_back({1u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, normal))});
        pipelineDesc.vertexAttributes.push_back({2u, 0u, RHI::VertexFormat::RGBA32F, static_cast<std::uint32_t>(offsetof(MeshVertex, tangent))});
        pipelineDesc.vertexAttributes.push_back({3u, 0u, RHI::VertexFormat::RG32F, static_cast<std::uint32_t>(offsetof(MeshVertex, texCoord0))});
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::None;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = true;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        pipelineDesc.blendStates.push_back({});
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    RHI::RHITexture *ForwardOpaquePass::GetOrCreateTexture(const ForwardPassContext &context, const std::string &texturePath)
    {
        if (texturePath.empty() || context.assetManager == nullptr || context.device == nullptr)
        {
            return nullptr;
        }

        const std::string normalizedPath = context.assetManager->NormalizePath(texturePath);
        const auto cached = m_TextureCache.find(normalizedPath);
        if (cached != m_TextureCache.end())
        {
            return cached->second.texture.get();
        }

        const std::shared_ptr<Texture> texture = context.assetManager->GetByPath<Texture>(normalizedPath);
        if (texture == nullptr || !texture->IsLoaded() || texture->rgba8Pixels.empty())
        {
            if (m_MissingTextureWarnings.insert(normalizedPath).second)
            {
                PHYSARA_CORE_WARN("Forward pass texture '{}' is not loaded; using fallback texture.", normalizedPath);
            }
            return nullptr;
        }

        const std::uint32_t mipLevels = ForwardOpaquePassDetail::CalculateMipLevels(texture->width, texture->height);
        TextureGPUResource resource{};
        resource.texture = context.device->CreateTexture(
            ForwardOpaquePassDetail::TextureDesc(texture->width, texture->height, texture->rgba8Pixels.data(), mipLevels));
        if (resource.texture == nullptr)
        {
            PHYSARA_CORE_ERROR("Forward pass failed to upload texture '{}'.", normalizedPath);
            return nullptr;
        }

        if (mipLevels > 1u)
        {
            context.commandList->GenerateMipmaps(resource.texture.get());
            resource.generatedMipmaps = true;
        }

        PHYSARA_CORE_INFO("Forward texture uploaded '{}': {}x{}, mips={}.",
                          normalizedPath,
                          texture->width,
                          texture->height,
                          mipLevels);
        auto [inserted, _] = m_TextureCache.emplace(normalizedPath, std::move(resource));
        return inserted->second.texture.get();
    }

    RHI::RHITexture *ForwardOpaquePass::GetFallbackWhiteTexture() const
    {
        return m_FallbackWhiteTexture.get();
    }

    RHI::RHITexture *ForwardOpaquePass::GetFallbackNormalTexture() const
    {
        return m_FallbackNormalTexture.get();
    }

    void ForwardOpaquePass::BindMaterial(const ForwardPassContext &context, const RenderDrawItem &item)
    {
        const ForwardOpaquePassDetail::MaterialGPUData material = ForwardOpaquePassDetail::BuildMaterial(item.submission.material);
        m_MaterialBuffer->UploadData(&material, sizeof(material));

        RHI::RHITexture *baseColor = GetOrCreateTexture(context, item.submission.material.baseColorTexture.path);
        RHI::RHITexture *metallicRoughness = GetOrCreateTexture(context, item.submission.material.metallicRoughnessTexture.path);
        RHI::RHITexture *normal = GetOrCreateTexture(context, item.submission.material.normalTexture.path);
        RHI::RHITexture *occlusion = GetOrCreateTexture(context, item.submission.material.occlusionTexture.path);
        RHI::RHITexture *emissive = GetOrCreateTexture(context, item.submission.material.emissiveTexture.path);

        context.commandList->SetTexture(ForwardOpaquePassDetail::BaseColorTextureBinding,
                                        baseColor != nullptr ? baseColor : GetFallbackWhiteTexture(),
                                        m_LinearRepeatSampler.get());
        context.commandList->SetTexture(ForwardOpaquePassDetail::MetallicRoughnessTextureBinding,
                                        metallicRoughness != nullptr ? metallicRoughness : GetFallbackWhiteTexture(),
                                        m_LinearRepeatSampler.get());
        context.commandList->SetTexture(ForwardOpaquePassDetail::NormalTextureBinding,
                                        normal != nullptr ? normal : GetFallbackNormalTexture(),
                                        m_LinearRepeatSampler.get());
        context.commandList->SetTexture(ForwardOpaquePassDetail::OcclusionTextureBinding,
                                        occlusion != nullptr ? occlusion : GetFallbackWhiteTexture(),
                                        m_LinearRepeatSampler.get());
        context.commandList->SetTexture(ForwardOpaquePassDetail::EmissiveTextureBinding,
                                        emissive != nullptr ? emissive : GetFallbackWhiteTexture(),
                                        m_LinearRepeatSampler.get());
    }

    ForwardOpaquePass::MeshGPUPrimitive *ForwardOpaquePass::GetOrCreateMeshPrimitive(const ForwardPassContext &context, const RenderDrawItem &item)
    {
        if (context.assetManager == nullptr || context.device == nullptr)
        {
            return nullptr;
        }

        const std::string primitiveKey = BuildMeshPrimitiveKey(item);
        const auto cached = m_MeshCache.find(primitiveKey);
        if (cached != m_MeshCache.end())
        {
            return &cached->second;
        }

        const std::string meshResourcePath = BuildMeshResourcePath(item);
        const std::shared_ptr<Mesh> mesh = context.assetManager->GetByPath<Mesh>(meshResourcePath);
        if (mesh == nullptr || item.submission.primitiveIndex >= mesh->primitives.size())
        {
            if (m_MissingMeshWarnings.insert(primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Forward pass skipped mesh '{}': resource not found or primitive index out of range. normalized='{}'.",
                                  primitiveKey,
                                  context.assetManager->NormalizePath(meshResourcePath));
            }
            return nullptr;
        }

        const MeshPrimitive &primitive = mesh->primitives[item.submission.primitiveIndex];
        if (!primitive.HasGeometry())
        {
            if (m_MissingMeshWarnings.insert(primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Forward pass skipped mesh '{}': primitive has no decoded geometry.",
                                  primitiveKey);
            }
            return nullptr;
        }

        MeshGPUPrimitive gpuPrimitive{};
        gpuPrimitive.vertexBuffer = context.device->CreateBuffer(
            ForwardOpaquePassDetail::StaticBufferDesc(primitive.vertices, RHI::BufferUsage::Vertex));
        gpuPrimitive.indexBuffer = context.device->CreateBuffer(
            ForwardOpaquePassDetail::StaticBufferDesc(primitive.indices, RHI::BufferUsage::Index));
        gpuPrimitive.indexCount = static_cast<std::uint32_t>(primitive.indices.size());

        if (gpuPrimitive.vertexBuffer == nullptr || gpuPrimitive.indexBuffer == nullptr)
        {
            PHYSARA_CORE_ERROR("Forward pass failed to upload mesh '{}'.", primitiveKey);
            return nullptr;
        }

        PHYSARA_CORE_INFO("Forward mesh uploaded '{}': vertices={}, indices={}.",
                          primitiveKey,
                          primitive.vertices.size(),
                          primitive.indices.size());
        auto [inserted, _] = m_MeshCache.emplace(primitiveKey, std::move(gpuPrimitive));
        return &inserted->second;
    }

    void ForwardOpaquePass::DrawBucket(const ForwardPassContext &context, const std::vector<RenderDrawItem> &bucket)
    {
        for (const RenderDrawItem &item : bucket)
        {
            MeshGPUPrimitive *primitive = GetOrCreateMeshPrimitive(context, item);
            if (primitive == nullptr || primitive->indexCount == 0)
            {
                continue;
            }

            BindMaterial(context, item);
            context.commandList->SetVertexBuffer(0u, primitive->vertexBuffer.get());
            context.commandList->SetIndexBuffer(primitive->indexBuffer.get());
            context.commandList->DrawIndexed(primitive->indexCount, 1u, 0u, 0, item.objectIndex);
            if (!m_LoggedFirstDraw)
            {
                PHYSARA_CORE_INFO("Forward draw submitted '{}': indices={}, objectIndex={}.",
                                  BuildMeshPrimitiveKey(item),
                                  primitive->indexCount,
                                  item.objectIndex);
                m_LoggedFirstDraw = true;
            }
        }
    }

    std::string ForwardOpaquePass::BuildMeshResourcePath(const RenderDrawItem &item)
    {
        return item.submission.meshPath + "#mesh/" + std::to_string(item.submission.meshIndex);
    }

    std::string ForwardOpaquePass::BuildMeshPrimitiveKey(const RenderDrawItem &item)
    {
        return BuildMeshResourcePath(item) + "#primitive/" + std::to_string(item.submission.primitiveIndex);
    }
}