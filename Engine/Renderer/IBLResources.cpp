#include "IBLResources.hpp"

#include <algorithm>
#include <exception>
#include <thread>
#include <utility>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

namespace Physara::Engine
{
    void IBLResources::Reset()
    {
        m_LoadedEnvironmentPath.clear();
        m_PendingPrecompute.reset();
        ReleaseGPUResources();
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
            if (!m_UsingPreview || m_PendingPrecompute == nullptr)
            {
                return true;
            }
        }

        if (m_LoadedEnvironmentPath != normalizedPath)
        {
            m_LoadedEnvironmentPath.clear();
            ReleaseGPUResources();
        }

        if (m_PendingPrecompute == nullptr || m_PendingPrecompute->path != normalizedPath)
        {
            StartPrecompute(normalizedPath);
            return false;
        }

        std::shared_ptr<IBLPrecomputeResult> result;
        bool finalResult = false;
        {
            std::lock_guard lock(m_PendingPrecompute->mutex);
            if (m_PendingPrecompute->finalFinished)
            {
                result = m_PendingPrecompute->finalResult;
                finalResult = true;
            }
            else if (!m_Ready && m_PendingPrecompute->previewFinished && m_PendingPrecompute->previewResult != nullptr)
            {
                result = m_PendingPrecompute->previewResult;
            }
            else
            {
                return m_Ready;
            }
        }

        if (result == nullptr || !result->IsValid())
        {
            PHYSARA_CORE_WARN("IBL precompute for '{}' finished without valid resources.", normalizedPath.string());
            if (finalResult)
            {
                m_PendingPrecompute.reset();
            }
            return m_Ready;
        }

        if (!Upload(device, *result))
        {
            m_PendingPrecompute.reset();
            ReleaseGPUResources();
            return false;
        }

        m_LoadedEnvironmentPath = normalizedPath;
        m_UsingPreview = !finalResult;
        if (finalResult)
        {
            m_PendingPrecompute.reset();
        }
        m_Ready = true;
        PHYSARA_CORE_INFO("IBL {} resources ready for '{}': cube={}px, mips={}, brdf={}px.",
                          finalResult ? "final" : "preview",
                          normalizedPath.string(),
                          result->cubeSize,
                          result->specularMipCount,
                          result->brdfLutSize);
        return true;
    }

    void IBLResources::ReleaseGPUResources()
    {
        m_SpecularTexture.reset();
        m_BRDFLut.reset();
        m_IrradianceSH = {};
        m_SpecularMipCount = 0;
        m_UsingPreview = false;
        m_Ready = false;
    }

    void IBLResources::StartPrecompute(const std::filesystem::path &environmentPath)
    {
        auto pending = std::make_shared<PendingPrecompute>(environmentPath);
        m_PendingPrecompute = pending;
        PHYSARA_CORE_INFO("Queued IBL precompute for '{}'.", environmentPath.string());
        std::thread(
            [pending]()
            {
                try
                {
                    IBLPrecomputeSettings cacheOnlySettings{};
                    cacheOnlySettings.createIfMissing = false;
                    cacheOnlySettings.writeDebugOutputs = false;
                    std::shared_ptr<IBLPrecomputeResult> cached = IBLPrecompute::LoadOrCreate(pending->path, cacheOnlySettings);
                    if (cached != nullptr && cached->IsValid())
                    {
                        std::lock_guard lock(pending->mutex);
                        pending->finalResult = std::move(cached);
                        pending->finalFinished = true;
                        return;
                    }

                    std::thread(
                        [pending]()
                        {
                            try
                            {
                                PHYSARA_CORE_INFO("Starting final IBL cache build for '{}'.", pending->path.string());
                                std::shared_ptr<IBLPrecomputeResult> final = IBLPrecompute::LoadOrCreate(pending->path);
                                PHYSARA_CORE_INFO("Finished final IBL cache build for '{}': valid={}.",
                                                  pending->path.string(),
                                                  final != nullptr && final->IsValid());
                                std::lock_guard lock(pending->mutex);
                                pending->finalResult = std::move(final);
                                pending->finalFinished = true;
                            }
                            catch (const std::exception &exception)
                            {
                                PHYSARA_CORE_ERROR("Final IBL precompute for '{}' failed: {}", pending->path.string(), exception.what());
                                std::lock_guard lock(pending->mutex);
                                pending->finalFinished = true;
                            }
                        })
                        .detach();

                    IBLPrecomputeSettings previewSettings{};
                    previewSettings.cubeSize = 128u;
                    previewSettings.brdfLutSize = 128u;
                    previewSettings.specularSampleCount = 32u;
                    previewSettings.brdfSampleCount = 256u;
                    previewSettings.useCache = false;
                    previewSettings.writeCache = false;
                    previewSettings.writeDebugOutputs = false;
                    std::shared_ptr<IBLPrecomputeResult> preview = IBLPrecompute::LoadOrCreate(pending->path, previewSettings);
                    {
                        std::lock_guard lock(pending->mutex);
                        pending->previewResult = std::move(preview);
                        pending->previewFinished = true;
                    }
                }
                catch (const std::exception &exception)
                {
                    PHYSARA_CORE_ERROR("IBL precompute for '{}' failed: {}", pending->path.string(), exception.what());
                    std::lock_guard lock(pending->mutex);
                    pending->previewFinished = true;
                    pending->finalFinished = true;
                }
            })
            .detach();
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