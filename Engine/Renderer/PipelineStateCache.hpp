#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class PipelineStateCache final
    {
    public:
        PipelineStateCache() = default;
        explicit PipelineStateCache(RHI::RHIDevice *device);

        void SetDevice(RHI::RHIDevice *device);
        [[nodiscard]] RHI::RHIPipelineState *GetOrCreate(const RHI::RHIPipelineStateDesc &desc);
        void Clear();

    private:
        [[nodiscard]] static std::size_t HashDesc(const RHI::RHIPipelineStateDesc &desc);

    private:
        RHI::RHIDevice *m_Device{nullptr};
        std::unordered_map<std::size_t, std::unique_ptr<RHI::RHIPipelineState>> m_Cache{};
    };
}