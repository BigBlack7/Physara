#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    class RHIShader
    {
    public:
        virtual ~RHIShader() = default;

        virtual ShaderStage GetStage() const = 0;
        virtual bool IsValid() const = 0;
    };
}