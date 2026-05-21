#include "ForwardOpaquePass.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Renderer/UploadHasher.hpp>
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

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

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
            desc.size = MaxValue(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        std::uint64_t HashTextureSlot(std::uint64_t hash, const TextureSlot &slot)
        {
            hash = UploadHash::String(hash, slot.path);
            return UploadHash::Value(hash, slot.texCoord);
        }

        std::uint64_t HashMaterialComponent(std::uint64_t hash, const MaterialComponent &material, const AssetManager *assetManager)
        {
            hash = UploadHash::String(hash, material.materialPath);
            hash = UploadHash::Value(hash, material.shadingModel);
            hash = UploadHash::Value(hash, material.alphaMode);
            hash = UploadHash::Value(hash, material.doubleSided);
            hash = UploadHash::Value(hash, material.castShadow);
            hash = UploadHash::Value(hash, material.baseColor);
            hash = UploadHash::Value(hash, material.metallic);
            hash = UploadHash::Value(hash, material.roughness);
            hash = UploadHash::Value(hash, material.ambientOcclusion);
            hash = UploadHash::Value(hash, material.alphaCutoff);
            hash = UploadHash::Value(hash, material.emissiveColor);
            hash = UploadHash::Value(hash, material.emissiveLuminance);
            hash = UploadHash::Value(hash, material.normalScale);
            hash = HashTextureSlot(hash, material.baseColorTexture);
            hash = HashTextureSlot(hash, material.metallicRoughnessTexture);
            hash = HashTextureSlot(hash, material.normalTexture);
            hash = HashTextureSlot(hash, material.occlusionTexture);
            hash = HashTextureSlot(hash, material.emissiveTexture);

            bool baseColorHasTransparentPixels = false;
            if (assetManager != nullptr && material.baseColorTexture.IsBound())
            {
                const std::shared_ptr<Texture> texture = assetManager->GetByPath<Texture>(material.baseColorTexture.path);
                baseColorHasTransparentPixels = texture != nullptr && texture->hasTransparentPixels;
            }
            return UploadHash::Value(hash, baseColorHasTransparentPixels);
        }

        std::uint64_t HashMaterialLayout(const RenderProxy &renderProxy, const AssetManager *assetManager)
        {
            std::uint64_t hash = UploadHash::Offset;
            const RenderDrawBuckets &buckets = renderProxy.GetBuckets();
            const auto hashBucket = [&hash, assetManager](const std::vector<RenderDrawItem> &bucket)
            {
                hash = UploadHash::Value(hash, bucket.size());
                for (const RenderDrawItem &item : bucket)
                {
                    hash = UploadHash::Value(hash, item.objectIndex);
                    hash = UploadHash::Value(hash, item.sortKey);
                    if (item.submission != nullptr)
                    {
                        hash = HashMaterialComponent(hash, item.submission->material, assetManager);
                    }
                }
            };

            hashBucket(buckets.opaque);
            hashBucket(buckets.unlit);
            hashBucket(buckets.transparent);
            return hash;
        }

        ForwardMaterialGPUData BuildDefaultMaterial()
        {
            ForwardMaterialGPUData material{};
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

        float TextureCoordSetToShaderValue(const TextureSlot &slot)
        {
            return static_cast<float>(slot.texCoord > 0u ? 1u : 0u);
        }

        void ApplyRuntimeAlphaPolicy(MaterialComponent &materialComponent, const AssetManager *assetManager)
        {
            if (assetManager == nullptr || materialComponent.alphaMode != AlphaMode::Opaque || !materialComponent.baseColorTexture.IsBound())
            {
                return;
            }

            const std::shared_ptr<Texture> texture = assetManager->GetByPath<Texture>(materialComponent.baseColorTexture.path);
            if (texture != nullptr && texture->hasTransparentPixels)
            {
                materialComponent.alphaMode = AlphaMode::Mask;
                materialComponent.alphaCutoff = 0.5f;
            }
        }

        ForwardMaterialGPUData BuildMaterial(const MaterialComponent &component, const AssetManager *assetManager)
        {
            MaterialComponent materialComponent = component;
            ApplyRuntimeAlphaPolicy(materialComponent, assetManager);
            materialComponent.Sanitize();

            ForwardMaterialGPUData material{};
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
            material.textureCoordSets = glm::vec4(
                TextureCoordSetToShaderValue(materialComponent.baseColorTexture),
                TextureCoordSetToShaderValue(materialComponent.metallicRoughnessTexture),
                TextureCoordSetToShaderValue(materialComponent.normalTexture),
                TextureCoordSetToShaderValue(materialComponent.occlusionTexture));
            material.materialFlags = glm::vec4(
                materialComponent.doubleSided ? 1.f : 0.f,
                materialComponent.emissiveTexture.IsBound() ? 1.f : 0.f,
                TextureCoordSetToShaderValue(materialComponent.emissiveTexture),
                0.f);
            return material;
        }

        RHI::RHITextureDesc TextureDesc(std::uint32_t width, std::uint32_t height, const void *pixels, std::uint32_t mipLevels = 1u)
        {
            RHI::RHITextureDesc desc{};
            desc.width = MaxValue(width, 1u);
            desc.height = MaxValue(height, 1u);
            desc.mipLevels = MaxValue(mipLevels, 1u);
            desc.format = RHI::TextureFormat::RGBA8;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled;
            desc.initialData = pixels;
            return desc;
        }

        std::uint32_t CalculateMipLevels(std::uint32_t width, std::uint32_t height)
        {
            std::uint32_t levels = 1u;
            std::uint32_t size = MaxValue(width, height);
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

        void RecordBufferUpload(FrameStatistics *stats, std::uint64_t bytes)
        {
            if (stats != nullptr)
            {
                stats->bufferUploadBytes += bytes;
            }
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

    void ForwardOpaquePass::ExecuteBuckets(const ForwardPassContext &context, bool transparent)
    {
        if (context.commandList == nullptr || context.framebuffer == nullptr || context.renderPassDesc == nullptr ||
            context.frameData == nullptr || context.renderProxy == nullptr)
        {
            return;
        }

        EnsureDefaultTextures(context);
        EnsureFrameBuffers(context);
        RHI::RHIPipelineState *singleSidedPipeline = GetPipeline(context, RHI::CullMode::Back, transparent);
        RHI::RHIPipelineState *doubleSidedPipeline = GetPipeline(context, RHI::CullMode::None, transparent);

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

            context.commandList->SetUniformBuffer(ForwardOpaquePassDetail::CameraBinding, m_CameraBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::ObjectBinding, m_ObjectBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::MaterialBinding, m_MaterialBuffer.get());
            context.commandList->SetStorageBuffer(ForwardOpaquePassDetail::LightBinding, m_LightBuffer.get());

            ResetTextureBindings();
            context.commandList->SetPipelineState(singleSidedPipeline);
            if (transparent)
            {
                DrawBucket(context, context.renderProxy->GetBuckets().transparent, false);
            }
            else
            {
                DrawBucket(context, context.renderProxy->GetBuckets().opaque, false);
                DrawBucket(context, context.renderProxy->GetBuckets().unlit, false);
            }

            ResetTextureBindings();
            context.commandList->SetPipelineState(doubleSidedPipeline);
            if (transparent)
            {
                DrawBucket(context, context.renderProxy->GetBuckets().transparent, true);
            }
            else
            {
                DrawBucket(context, context.renderProxy->GetBuckets().opaque, true);
                DrawBucket(context, context.renderProxy->GetBuckets().unlit, true);
            }
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
            m_LastCameraUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        const std::uint32_t objectBufferSize = static_cast<std::uint32_t>(ForwardOpaquePassDetail::MaxValue<std::size_t>(frameData.objects.size(), 1u) * sizeof(ObjectData));
        if (m_ObjectBuffer == nullptr || m_ObjectBuffer->GetSize() < objectBufferSize)
        {
            m_ObjectBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(objectBufferSize, RHI::BufferUsage::Storage));
            m_LastObjectUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        const std::uint32_t lightBufferSize =
            static_cast<std::uint32_t>(sizeof(ForwardOpaquePassDetail::LightBufferHeader) +
                                       ForwardOpaquePassDetail::MaxValue<std::size_t>(frameData.lights.size(), 1u) * sizeof(LightData));
        if (m_LightBuffer == nullptr || m_LightBuffer->GetSize() < lightBufferSize)
        {
            m_LightBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(lightBufferSize, RHI::BufferUsage::Storage));
            m_LastLightUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        const std::uint32_t materialBufferSize =
            static_cast<std::uint32_t>(ForwardOpaquePassDetail::MaxValue<std::size_t>(frameData.objects.size(), 1u) * sizeof(ForwardMaterialGPUData));
        if (m_MaterialBuffer == nullptr || m_MaterialBuffer->GetSize() < materialBufferSize)
        {
            m_MaterialBuffer = context.device->CreateBuffer(
                ForwardOpaquePassDetail::DynamicBufferDesc(materialBufferSize, RHI::BufferUsage::Storage));
            m_LastMaterialUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        if (m_LastUploadedFrameIndex == frameData.frameIndex)
        {
            return;
        }

        const std::uint64_t cameraSignature = UploadHash::Value(UploadHash::Offset, frameData.camera);
        if (cameraSignature != m_LastCameraUploadSignature)
        {
            m_CameraBuffer->UploadData(&frameData.camera, sizeof(CameraData));
            ForwardOpaquePassDetail::RecordBufferUpload(context.stats, sizeof(CameraData));
            m_LastCameraUploadSignature = cameraSignature;
        }

        const std::uint64_t objectSignature = UploadHash::Vector(UploadHash::Offset, frameData.objects);
        if (!frameData.objects.empty() && objectSignature != m_LastObjectUploadSignature)
        {
            m_ObjectBuffer->UploadData(frameData.objects.data(), objectBufferSize);
            ForwardOpaquePassDetail::RecordBufferUpload(context.stats, objectBufferSize);
        }
        m_LastObjectUploadSignature = objectSignature;

        ForwardOpaquePassDetail::LightBufferHeader lightHeader{};
        lightHeader.lightCount = static_cast<std::uint32_t>(frameData.lights.size());
        std::uint64_t lightSignature = UploadHash::Value(UploadHash::Offset, lightHeader);
        lightSignature = UploadHash::Vector(lightSignature, frameData.lights);
        if (lightSignature != m_LastLightUploadSignature)
        {
            m_LightBuffer->UploadData(&lightHeader, sizeof(lightHeader));
            ForwardOpaquePassDetail::RecordBufferUpload(context.stats, sizeof(lightHeader));
            if (!frameData.lights.empty())
            {
                const std::uint32_t lightBytes = static_cast<std::uint32_t>(frameData.lights.size() * sizeof(LightData));
                m_LightBuffer->UploadData(
                    frameData.lights.data(),
                    lightBytes,
                    sizeof(lightHeader));
                ForwardOpaquePassDetail::RecordBufferUpload(context.stats, lightBytes);
            }
            m_LastLightUploadSignature = lightSignature;
        }

        const std::uint64_t materialSignature = context.renderProxy != nullptr
                                                   ? ForwardOpaquePassDetail::HashMaterialLayout(*context.renderProxy, context.assetManager)
                                                   : UploadHash::Offset;
        if (materialSignature == m_LastMaterialUploadSignature)
        {
            m_LastUploadedFrameIndex = frameData.frameIndex;
            return;
        }

        m_MaterialUploadScratch.assign(
            ForwardOpaquePassDetail::MaxValue<std::size_t>(frameData.objects.size(), 1u),
            ForwardOpaquePassDetail::BuildDefaultMaterial());
        auto &materials = m_MaterialUploadScratch;
        const auto fillMaterials = [&materials, &context](const std::vector<RenderDrawItem> &bucket)
        {
            for (const RenderDrawItem &item : bucket)
            {
                if (item.submission != nullptr && item.objectIndex < materials.size())
                {
                    materials[item.objectIndex] = ForwardOpaquePassDetail::BuildMaterial(item.submission->material, context.assetManager);
                }
            }
        };

        if (context.renderProxy != nullptr)
        {
            const RenderDrawBuckets &buckets = context.renderProxy->GetBuckets();
            fillMaterials(buckets.opaque);
            fillMaterials(buckets.unlit);
            fillMaterials(buckets.transparent);
        }
        m_MaterialBuffer->UploadData(materials.data(), static_cast<std::uint32_t>(materials.size() * sizeof(ForwardMaterialGPUData)));
        ForwardOpaquePassDetail::RecordBufferUpload(
            context.stats,
            static_cast<std::uint64_t>(materials.size() * sizeof(ForwardMaterialGPUData)));
        m_LastMaterialUploadSignature = materialSignature;
        m_LastUploadedFrameIndex = frameData.frameIndex;
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
            desc.anisotropy = static_cast<float>(ForwardOpaquePassDetail::MaxValue(context.device->GetMaxAnisotropy(), 1));
            m_LinearRepeatSampler = context.device->CreateSampler(desc);
        }

        if (m_FallbackWhiteTexture == nullptr)
        {
            const std::uint8_t white[4]{255u, 255u, 255u, 255u};
            m_FallbackWhiteTexture = context.device->CreateTexture(ForwardOpaquePassDetail::TextureDesc(1u, 1u, white));
        }

        if (m_FallbackNormalTexture == nullptr)
        {
            const std::uint8_t normal[4]{128u, 128u, 255u, 255u};
            m_FallbackNormalTexture = context.device->CreateTexture(ForwardOpaquePassDetail::TextureDesc(1u, 1u, normal));
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

        if (context.stats != nullptr)
        {
            ++context.stats->textureUploads;
            context.stats->textureUploadBytes += texture->rgba8Pixels.size();
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
        if (item.submission == nullptr)
        {
            return;
        }

        const MaterialComponent &material = item.submission->material;
        RHI::RHITexture *baseColor = GetOrCreateTexture(context, material.baseColorTexture.path);
        RHI::RHITexture *metallicRoughness = GetOrCreateTexture(context, material.metallicRoughnessTexture.path);
        RHI::RHITexture *normal = GetOrCreateTexture(context, material.normalTexture.path);
        RHI::RHITexture *occlusion = GetOrCreateTexture(context, material.occlusionTexture.path);
        RHI::RHITexture *emissive = GetOrCreateTexture(context, material.emissiveTexture.path);
        RHI::RHITexture *textures[5]{
            baseColor != nullptr ? baseColor : GetFallbackWhiteTexture(),
            metallicRoughness != nullptr ? metallicRoughness : GetFallbackWhiteTexture(),
            normal != nullptr ? normal : GetFallbackNormalTexture(),
            occlusion != nullptr ? occlusion : GetFallbackWhiteTexture(),
            emissive != nullptr ? emissive : GetFallbackWhiteTexture()};
        const std::uint32_t bindings[5]{
            ForwardOpaquePassDetail::BaseColorTextureBinding,
            ForwardOpaquePassDetail::MetallicRoughnessTextureBinding,
            ForwardOpaquePassDetail::NormalTextureBinding,
            ForwardOpaquePassDetail::OcclusionTextureBinding,
            ForwardOpaquePassDetail::EmissiveTextureBinding};

        RHI::RHISampler *sampler = m_LinearRepeatSampler.get();
        for (std::size_t i = 0; i < 5u; ++i)
        {
            if (m_BoundTextures[i] == textures[i] && m_BoundSampler == sampler)
            {
                continue;
            }

            context.commandList->SetTexture(bindings[i], textures[i], sampler);
            m_BoundTextures[i] = textures[i];
        }
        m_BoundSampler = sampler;
    }

    ForwardOpaquePass::MeshGPUPrimitive *ForwardOpaquePass::GetOrCreateMeshPrimitive(const ForwardPassContext &context, const RenderDrawItem &item)
    {
        if (context.assetManager == nullptr || context.device == nullptr)
        {
            return nullptr;
        }
        if (item.submission == nullptr)
        {
            return nullptr;
        }

        const auto cached = m_MeshCache.find(item.primitiveKey);
        if (cached != m_MeshCache.end())
        {
            return &cached->second;
        }

        const std::string meshResourcePath = BuildMeshResourcePath(item);
        const std::shared_ptr<Mesh> mesh = context.assetManager->GetByPath<Mesh>(meshResourcePath);
        if (mesh == nullptr || item.submission->primitiveIndex >= mesh->primitives.size())
        {
            if (m_MissingMeshWarnings.insert(item.primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Forward pass skipped mesh '{}': resource not found or primitive index out of range. normalized='{}'.",
                                  BuildMeshPrimitiveDebugName(item),
                                  context.assetManager->NormalizePath(meshResourcePath));
            }
            return nullptr;
        }

        const MeshPrimitive &primitive = mesh->primitives[item.submission->primitiveIndex];
        if (!primitive.HasGeometry())
        {
            if (m_MissingMeshWarnings.insert(item.primitiveKey).second)
            {
                PHYSARA_CORE_WARN("Forward pass skipped mesh '{}': primitive has no decoded geometry.",
                                  BuildMeshPrimitiveDebugName(item));
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
            PHYSARA_CORE_ERROR("Forward pass failed to upload mesh '{}'.", BuildMeshPrimitiveDebugName(item));
            return nullptr;
        }

        if (context.stats != nullptr)
        {
            const std::uint64_t vertexBytes = static_cast<std::uint64_t>(primitive.vertices.size() * sizeof(MeshVertex));
            const std::uint64_t indexBytes = static_cast<std::uint64_t>(primitive.indices.size() * sizeof(std::uint32_t));
            ++context.stats->meshUploads;
            context.stats->meshUploadBytes += vertexBytes + indexBytes;
        }

        PHYSARA_CORE_INFO("Forward mesh uploaded '{}': vertices={}, indices={}.",
                          BuildMeshPrimitiveDebugName(item),
                          primitive.vertices.size(),
                          primitive.indices.size());
        auto [inserted, _] = m_MeshCache.emplace(item.primitiveKey, std::move(gpuPrimitive));
        return &inserted->second;
    }

    void ForwardOpaquePass::DrawBucket(const ForwardPassContext &context, const std::vector<RenderDrawItem> &bucket, bool drawDoubleSided)
    {
        for (std::size_t i = 0; i < bucket.size();)
        {
            const RenderDrawItem &item = bucket[i];
            if (item.doubleSided != drawDoubleSided)
            {
                ++i;
                continue;
            }

            MeshGPUPrimitive *primitive = GetOrCreateMeshPrimitive(context, item);
            if (primitive == nullptr || primitive->indexCount == 0)
            {
                ++i;
                continue;
            }

            std::uint32_t instanceCount = 1u;
            while (i + instanceCount < bucket.size() &&
                   CanInstanceTogether(context, item, bucket[i + instanceCount], instanceCount))
            {
                ++instanceCount;
            }

            BindMaterial(context, item);
            context.commandList->SetVertexBuffer(0u, primitive->vertexBuffer.get());
            context.commandList->SetIndexBuffer(primitive->indexBuffer.get());
            context.commandList->DrawIndexed(primitive->indexCount, instanceCount, 0u, 0, item.objectIndex);
            if (context.stats != nullptr)
            {
                ++context.stats->drawCalls;
                context.stats->instances += instanceCount;
                context.stats->triangles += static_cast<std::uint64_t>(primitive->indexCount / 3u) * instanceCount;
            }
            if (!m_LoggedFirstDraw)
            {
                PHYSARA_CORE_INFO("Forward draw submitted '{}': indices={}, objectIndex={}, instances={}.",
                                  BuildMeshPrimitiveDebugName(item),
                                  primitive->indexCount,
                                  item.objectIndex,
                                  instanceCount);
                m_LoggedFirstDraw = true;
            }

            i += instanceCount;
        }
    }

    bool ForwardOpaquePass::CanInstanceTogether(const ForwardPassContext &context, const RenderDrawItem &first, const RenderDrawItem &candidate, std::uint32_t instanceOffset)
    {
        (void)context;
        if (candidate.objectIndex != first.objectIndex + instanceOffset)
        {
            return false;
        }

        if (candidate.sortKey != first.sortKey)
        {
            return false;
        }

        if (candidate.doubleSided != first.doubleSided)
        {
            return false;
        }

        return candidate.primitiveKey == first.primitiveKey;
    }

    void ForwardOpaquePass::ResetTextureBindings()
    {
        for (RHI::RHITexture *&texture : m_BoundTextures)
        {
            texture = nullptr;
        }
        m_BoundSampler = nullptr;
    }

    std::string ForwardOpaquePass::BuildMeshResourcePath(const RenderDrawItem &item)
    {
        if (item.submission == nullptr)
        {
            return {};
        }

        return item.submission->meshPath + "#mesh/" + std::to_string(item.submission->meshIndex);
    }

    std::string ForwardOpaquePass::BuildMeshPrimitiveDebugName(const RenderDrawItem &item)
    {
        if (item.submission == nullptr)
        {
            return {};
        }

        return BuildMeshResourcePath(item) + "#primitive/" + std::to_string(item.submission->primitiveIndex);
    }

}