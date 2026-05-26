#include "IBLResources.hpp"

#include <algorithm>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>
#include <Engine/RHI/Resource/RHIShader.hpp>
#include <Engine/Resource/Loaders/ShaderLoader.hpp>

namespace Physara::Engine
{
    namespace IBLResourcesDetail
    {
        std::unique_ptr<RHI::RHITexture> CreateBRDFLutWithCompute(RHI::RHIDevice *device, std::uint32_t size)
        {
            if (device == nullptr)
            {
                return {};
            }

            ShaderSource source = ShaderLoader::Load({
                RHI::ShaderStage::Compute,
                "Shaders/Passes/IBL/BRDFIntegrate.comp",
                ShaderFeature::None,
                {}});
            std::unique_ptr<RHI::RHIShader> shader = device->CreateShader(RHI::ShaderStage::Compute, source.source);
            if (shader == nullptr || !shader->IsValid())
            {
                return {};
            }

            RHI::RHIPipelineStateDesc pipelineDesc{};
            pipelineDesc.computeShader = shader.get();
            std::unique_ptr<RHI::RHIPipelineState> pipeline = device->CreatePipelineState(pipelineDesc);
            if (pipeline == nullptr || !pipeline->IsValid())
            {
                return {};
            }

            RHI::RHITextureDesc desc{};
            desc.width = size;
            desc.height = size;
            desc.format = RHI::TextureFormat::RG16F;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;
            desc.mipLevels = 1u;
            desc.arrayLayers = 1u;
            std::unique_ptr<RHI::RHITexture> texture = device->CreateTexture(desc);
            if (texture == nullptr)
            {
                return {};
            }

            RHI::RHICommandList *commandList = device->GetCommandList();
            if (commandList == nullptr)
            {
                return {};
            }

            commandList->BeginDebugLabel("IBL BRDFIntegrate.compute");
            commandList->SetPipelineState(pipeline.get());
            commandList->SetStorageTexture(0u, texture.get(), 0u, 0u, RHI::StorageTextureAccess::WriteOnly);
            commandList->Dispatch((size + 7u) / 8u, (size + 7u) / 8u, 1u);
            commandList->TextureBarrier(texture.get(), RHI::ShaderStage::Compute, RHI::ShaderStage::Fragment);
            commandList->EndDebugLabel();
            PHYSARA_CORE_INFO("Generated BRDF integration LUT with GPU compute: {}x{}.", size, size);
            return texture;
        }
    }

    void IBLResources::Reset()
    {
        m_LoadedEnvironmentPath.clear();
        m_SpecularTexture.reset();
        m_BRDFLut.reset();
        m_IrradianceSH = {};
        m_SpecularMipCount = 0;
        m_Ready = false;
    }

    void IBLResources::Invalidate()
    {
        Reset();
    }

    bool IBLResources::Ensure(RHI::RHIDevice *device, const std::filesystem::path &environmentPath)
    {
        if (device == nullptr || environmentPath.empty())
        {
            Reset();
            return false;
        }

        const std::filesystem::path normalizedPath = environmentPath.lexically_normal();
        if (m_Ready && m_LoadedEnvironmentPath == normalizedPath)
        {
            return true;
        }

        Reset();
        std::shared_ptr<IBLPrecomputeResult> result = IBLPrecompute::LoadOrCreate(normalizedPath);
        if (result == nullptr || !result->IsValid())
        {
            return false;
        }

        if (!Upload(device, *result))
        {
            Reset();
            return false;
        }

        m_LoadedEnvironmentPath = normalizedPath;
        m_Ready = true;
        PHYSARA_CORE_INFO("IBL resources ready for '{}': cube={}px, mips={}, brdf={}px.",
                          normalizedPath.string(),
                          result->cubeSize,
                          result->specularMipCount,
                          result->brdfLutSize);
        return true;
    }

    bool IBLResources::Upload(RHI::RHIDevice *device, const IBLPrecomputeResult &result)
    {
        RHI::RHITextureDesc cubeDesc{};
        cubeDesc.width = result.cubeSize;
        cubeDesc.height = result.cubeSize;
        cubeDesc.format = RHI::TextureFormat::RGBA16F;
        cubeDesc.dimension = RHI::TextureDimension::TexCube;
        cubeDesc.usage = RHI::TextureUsage::Sampled;
        cubeDesc.mipLevels = result.specularMipCount;
        cubeDesc.arrayLayers = 6u;
        m_SpecularTexture = device->CreateTexture(cubeDesc);
        if (m_SpecularTexture == nullptr)
        {
            return false;
        }

        for (std::uint32_t mip = 0; mip < result.specularMipCount; ++mip)
        {
            for (std::uint32_t face = 0; face < 6u; ++face)
            {
                const IBLCubeFace &cubeFace = result.specularMipChain[mip][face];
                if (!cubeFace.rgba32f.empty())
                {
                    m_SpecularTexture->Upload(mip, face, cubeFace.rgba32f.data(), 0u);
                }
            }
        }

        m_BRDFLut = IBLResourcesDetail::CreateBRDFLutWithCompute(device, result.brdfLutSize);
        if (m_BRDFLut == nullptr)
        {
            RHI::RHITextureDesc brdfDesc{};
            brdfDesc.width = result.brdfLutSize;
            brdfDesc.height = result.brdfLutSize;
            brdfDesc.format = RHI::TextureFormat::RG16F;
            brdfDesc.dimension = RHI::TextureDimension::Tex2D;
            brdfDesc.usage = RHI::TextureUsage::Sampled;
            brdfDesc.mipLevels = 1u;
            brdfDesc.arrayLayers = 1u;
            brdfDesc.initialData = result.brdfLutRG32F.data();
            m_BRDFLut = device->CreateTexture(brdfDesc);
        }
        if (m_BRDFLut == nullptr)
        {
            m_SpecularTexture.reset();
            return false;
        }

        m_IrradianceSH = result.irradianceSH;
        m_SpecularMipCount = result.specularMipCount;
        return true;
    }
}