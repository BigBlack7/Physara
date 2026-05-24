#include "IBLResources.hpp"

#include <algorithm>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

namespace Physara::Engine
{
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