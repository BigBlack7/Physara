#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>
#include <Engine/RHI/Descriptors/RHISamplerDesc.hpp>

namespace Physara::RHI
{
    class RHISampler
    {
    public:
        virtual ~RHISampler() = default;

        virtual const RHISamplerDesc &GetDesc() const = 0;
    };
}