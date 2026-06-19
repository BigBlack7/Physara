#include "GPUScene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <Engine/Renderer/IBLResources.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Renderer/UploadHasher.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Texture.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::Engine
{
    namespace GPUSceneDetail
    {
        struct LightBufferHeader
        {
            std::uint32_t lightCount{0};
            std::uint32_t padding0{0};
            std::uint32_t padding1{0};
            std::uint32_t padding2{0};
        };
        static_assert(sizeof(LightBufferHeader) % 16 == 0);

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        RHI::RHIBufferDesc DynamicBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = MaxValue(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        bool GetBaseColorTransparentPixels(const MaterialComponent &material, const AssetManager *assetManager)
        {
            if (assetManager == nullptr || !material.baseColorTexture.IsBound())
            {
                return false;
            }

            const std::shared_ptr<Texture> texture = assetManager->GetByPath<Texture>(material.baseColorTexture.path);
            return texture != nullptr && texture->hasTransparentPixels;
        }

        std::uint64_t HashMaterialTable(const FrameData &frameData, const AssetManager *assetManager)
        {
            std::uint64_t hash = UploadHash::Offset;
            hash = UploadHash::Value(hash, frameData.materials.size());
            for (std::size_t i = 0; i < frameData.materials.size(); ++i)
            {
                const MaterialInstanceId materialId =
                    i < frameData.materialInstanceIds.size() ? frameData.materialInstanceIds[i] : InvalidMaterialInstanceId;
                const std::uint64_t materialSignature =
                    i < frameData.materialSignatures.size() ? frameData.materialSignatures[i] : 0u;
                const bool baseColorHasTransparentPixels = GetBaseColorTransparentPixels(frameData.materials[i], assetManager);
                hash = UploadHash::Value(hash, materialId);
                hash = UploadHash::Value(hash, materialSignature);
                hash = UploadHash::Value(hash, baseColorHasTransparentPixels);
            }
            return hash;
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
            return static_cast<float>(GPUValue(model == ShadingModel::Unlit ? ShadingModelGPU::Unlit : ShadingModelGPU::Lit));
        }

        float AlphaModeToShaderValue(AlphaMode mode)
        {
            switch (mode)
            {
            case AlphaMode::Mask:
                return static_cast<float>(GPUValue(AlphaModeGPU::Mask));
            case AlphaMode::Blend:
                return static_cast<float>(GPUValue(AlphaModeGPU::Blend));
            case AlphaMode::Opaque:
            default:
                return static_cast<float>(GPUValue(AlphaModeGPU::Opaque));
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

        MaterialGPUData BuildMaterial(const MaterialComponent &component, const AssetManager *assetManager)
        {
            MaterialComponent materialComponent = component;
            ApplyRuntimeAlphaPolicy(materialComponent, assetManager);
            materialComponent.Sanitize();

            MaterialGPUData material{};
            material.baseColor = materialComponent.baseColor;
            material.emissiveColorLuminance = glm::vec4(materialComponent.emissiveColor, materialComponent.emissiveLuminance);
            material.metallicRoughnessReflectanceAO = glm::vec4(
                materialComponent.metallic,
                materialComponent.roughness,
                materialComponent.reflectance,
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
                materialComponent.flipNormalY ? 1.f : 0.f);
            material.textureInfluences = glm::vec4(
                materialComponent.metallicTextureInfluence,
                materialComponent.roughnessTextureInfluence,
                materialComponent.ambientOcclusionTextureInfluence,
                0.f);
            return material;
        }

        FrameUniforms BuildFrameUniforms(
            const FrameData &frameData,
            const IBLResources *iblResources,
            float environmentExposureCompensation,
            std::uint32_t debugView)
        {
            FrameUniforms uniforms{};
            uniforms.camera = frameData.camera;
            uniforms.shadow = frameData.shadow;
            uniforms.debugParams.x = static_cast<float>(debugView);
            if (iblResources == nullptr || !iblResources->IsReady())
            {
                return uniforms;
            }

            const std::array<glm::vec4, 9> &sh = iblResources->GetIrradianceSH();
            for (std::size_t i = 0; i < sh.size(); ++i)
            {
                uniforms.ibl.irradianceSH[i] = sh[i];
            }
            uniforms.ibl.params = glm::vec4(
                std::exp2(environmentExposureCompensation) * frameData.camera.exposure.x,
                static_cast<float>(iblResources->GetSpecularMipCount() > 0u ? iblResources->GetSpecularMipCount() - 1u : 0u),
                1.f,
                0.f);
            return uniforms;
        }
    }

    void GPUScene::Reset()
    {
        m_FrameUniformAllocation = {};
        m_ObjectAllocation = {};
        m_LightAllocation = {};
        m_ForwardInstanceObjectIndexAllocation = {};
        m_ShadowInstanceObjectIndexAllocation = {};
        m_ObjectCount = 0u;
        m_LightCount = 0u;
        m_MaterialCount = 0u;
        m_ForwardInstanceObjectIndexCount = 0u;
        m_ShadowInstanceObjectIndexCount = 0u;
    }

    void GPUScene::Release()
    {
        Reset();
        m_MaterialBuffer.reset();
        m_LightUploadScratch.clear();
        m_MaterialUploadScratch.clear();
        m_LastMaterialUploadSignature = std::numeric_limits<std::uint64_t>::max();
    }

    void GPUScene::UploadFrame(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const FrameData &frameData,
        const RenderProxy &renderProxy,
        const AssetManager *assetManager,
        FrameStatistics *stats)
    {
        Reset();
        m_ObjectAllocation = UploadObjectTable(device, allocator, frameData.objects, stats);
        m_LightAllocation = UploadLightTable(device, allocator, frameData.lights, stats);
        UploadMaterialTable(device, frameData, assetManager, stats);
        m_ForwardInstanceObjectIndexAllocation = UploadInstanceObjectIndices(
            device,
            allocator,
            renderProxy.GetCommands().instanceObjectIndices,
            stats);

        m_ObjectCount = static_cast<std::uint32_t>(frameData.objects.size());
        m_LightCount = static_cast<std::uint32_t>(frameData.lights.size());
        m_MaterialCount = static_cast<std::uint32_t>(frameData.materials.size());
        m_ForwardInstanceObjectIndexCount = static_cast<std::uint32_t>(renderProxy.GetCommands().instanceObjectIndices.size());
    }

    void GPUScene::UploadFrameUniforms(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const FrameData &frameData,
        const IBLResources *iblResources,
        float environmentExposureCompensation,
        std::uint32_t debugView,
        FrameStatistics *stats)
    {
        const FrameUniforms uniforms = GPUSceneDetail::BuildFrameUniforms(
            frameData,
            iblResources,
            environmentExposureCompensation,
            debugView);
        m_FrameUniformAllocation = allocator.Upload(device, uniforms, stats);
    }

    void GPUScene::UploadShadowInstanceObjectIndices(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const std::vector<std::uint32_t> &instanceObjectIndices,
        FrameStatistics *stats)
    {
        m_ShadowInstanceObjectIndexAllocation = UploadInstanceObjectIndices(device, allocator, instanceObjectIndices, stats);
        m_ShadowInstanceObjectIndexCount = static_cast<std::uint32_t>(instanceObjectIndices.size());
    }

    FrameUploadAllocation GPUScene::UploadObjectTable(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const std::vector<ObjectData> &objects,
        FrameStatistics *stats)
    {
        if (objects.empty())
        {
            const ObjectData defaultObject{};
            return allocator.Upload(device, defaultObject, stats);
        }

        return allocator.Upload(
            device,
            objects.data(),
            static_cast<std::uint32_t>(objects.size() * sizeof(ObjectData)),
            stats);
    }

    FrameUploadAllocation GPUScene::UploadInstanceObjectIndices(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const std::vector<std::uint32_t> &indices,
        FrameStatistics *stats)
    {
        if (indices.empty())
        {
            const std::uint32_t defaultIndex = 0u;
            return allocator.Upload(device, defaultIndex, stats);
        }

        return allocator.Upload(
            device,
            indices.data(),
            static_cast<std::uint32_t>(indices.size() * sizeof(std::uint32_t)),
            stats);
    }

    FrameUploadAllocation GPUScene::UploadLightTable(
        RHI::RHIDevice &device,
        FrameUploadAllocator &allocator,
        const std::vector<LightData> &lights,
        FrameStatistics *stats)
    {
        const std::uint32_t lightCount = static_cast<std::uint32_t>(lights.size());
        const std::uint32_t lightBytes = static_cast<std::uint32_t>(lights.size() * sizeof(LightData));
        const std::uint32_t uploadBytes = static_cast<std::uint32_t>(sizeof(GPUSceneDetail::LightBufferHeader) + std::max(lightBytes, static_cast<std::uint32_t>(sizeof(LightData))));

        if (m_LightUploadScratch.size() < uploadBytes)
        {
            m_LightUploadScratch.resize(uploadBytes);
        }
        GPUSceneDetail::LightBufferHeader header{};
        header.lightCount = lightCount;
        std::memcpy(m_LightUploadScratch.data(), &header, sizeof(header));
        if (!lights.empty())
        {
            std::memcpy(m_LightUploadScratch.data() + sizeof(header), lights.data(), lightBytes);
        }
        else
        {
            LightData defaultLight{};
            std::memcpy(m_LightUploadScratch.data() + sizeof(header), &defaultLight, sizeof(defaultLight));
        }

        return allocator.Upload(device, m_LightUploadScratch.data(), uploadBytes, stats);
    }

    void GPUScene::UploadMaterialTable(
        RHI::RHIDevice &device,
        const FrameData &frameData,
        const AssetManager *assetManager,
        FrameStatistics *stats)
    {
        const std::vector<MaterialComponent> &materials = frameData.materials;
        const std::uint32_t materialBufferSize = static_cast<std::uint32_t>(
            GPUSceneDetail::MaxValue<std::size_t>(materials.size(), 1u) * sizeof(MaterialGPUData));
        if (m_MaterialBuffer == nullptr || m_MaterialBuffer->GetSize() < materialBufferSize)
        {
            m_MaterialBuffer = device.CreateBuffer(
                GPUSceneDetail::DynamicBufferDesc(materialBufferSize, RHI::BufferUsage::Storage));
            m_LastMaterialUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }
        if (m_MaterialBuffer == nullptr)
        {
            return;
        }

        const std::uint64_t materialSignature = GPUSceneDetail::HashMaterialTable(frameData, assetManager);
        if (materialSignature == m_LastMaterialUploadSignature)
        {
            return;
        }

        m_MaterialUploadScratch.assign(
            GPUSceneDetail::MaxValue<std::size_t>(materials.size(), 1u),
            GPUSceneDetail::BuildDefaultMaterial());
        for (std::size_t i = 0; i < materials.size(); ++i)
        {
            m_MaterialUploadScratch[i] = GPUSceneDetail::BuildMaterial(materials[i], assetManager);
        }

        const std::uint32_t uploadBytes = static_cast<std::uint32_t>(m_MaterialUploadScratch.size() * sizeof(MaterialGPUData));
        m_MaterialBuffer->UploadData(m_MaterialUploadScratch.data(), uploadBytes);
        if (stats != nullptr)
        {
            stats->bufferUploadBytes += uploadBytes;
            ++stats->bufferUploadChunks;
        }
        m_LastMaterialUploadSignature = materialSignature;
    }
}