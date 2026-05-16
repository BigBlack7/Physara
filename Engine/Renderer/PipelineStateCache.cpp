#include "PipelineStateCache.hpp"

#include <functional>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>

namespace Physara::Engine
{
    namespace PipelineStateCacheDetail
    {
        void HashCombine(std::size_t &seed, std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }
    }

    PipelineStateCache::PipelineStateCache(RHI::RHIDevice *device)
        : m_Device(device)
    {
    }

    void PipelineStateCache::SetDevice(RHI::RHIDevice *device)
    {
        if (m_Device == device)
        {
            return;
        }

        m_Device = device;
        Clear();
    }

    RHI::RHIPipelineState *PipelineStateCache::GetOrCreate(const RHI::RHIPipelineStateDesc &desc)
    {
        if (m_Device == nullptr)
        {
            PHYSARA_CORE_ERROR("PipelineStateCache cannot create a PSO without an RHIDevice.");
            return nullptr;
        }

        const std::size_t hash = HashDesc(desc);
        const auto cached = m_Cache.find(hash);
        if (cached != m_Cache.end())
        {
            return cached->second.get();
        }

        std::unique_ptr<RHI::RHIPipelineState> pipeline = m_Device->CreatePipelineState(desc);
        if (pipeline == nullptr || !pipeline->IsValid())
        {
            PHYSARA_CORE_ERROR("Failed to create pipeline state.");
            return nullptr;
        }

        RHI::RHIPipelineState *result = pipeline.get();
        m_Cache.emplace(hash, std::move(pipeline));
        return result;
    }

    void PipelineStateCache::Clear()
    {
        m_Cache.clear();
    }

    std::size_t PipelineStateCache::HashDesc(const RHI::RHIPipelineStateDesc &desc)
    {
        std::size_t seed = std::hash<const void *>{}(desc.vertexShader);
        PipelineStateCacheDetail::HashCombine(seed, std::hash<const void *>{}(desc.fragmentShader));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<const void *>{}(desc.computeShader));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<const void *>{}(desc.renderPassDesc));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(desc.topology)));

        PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(desc.rasterizerState.cullMode)));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(desc.rasterizerState.polygonMode)));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<float>{}(desc.rasterizerState.depthBias));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<float>{}(desc.rasterizerState.depthBiasSlope));

        PipelineStateCacheDetail::HashCombine(seed, std::hash<bool>{}(desc.depthStencilState.depthTest));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<bool>{}(desc.depthStencilState.depthWrite));
        PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(desc.depthStencilState.compareOp)));

        for (const RHI::RHIVertexAttribute &attribute : desc.vertexAttributes)
        {
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(attribute.location));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(attribute.binding));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(attribute.format)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(attribute.offset));
        }

        for (const RHI::RHIVertexBinding &binding : desc.vertexBindings)
        {
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(binding.binding));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(binding.stride));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(binding.instanceDivisor));
        }

        for (const RHI::RHIBlendState &blend : desc.blendStates)
        {
            PipelineStateCacheDetail::HashCombine(seed, std::hash<bool>{}(blend.blendEnable));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.srcColor)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.dstColor)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.colorOp)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.srcAlpha)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.dstAlpha)));
            PipelineStateCacheDetail::HashCombine(seed, std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(blend.alphaOp)));
        }

        return seed;
    }
}