#pragma once

#include <cstdint>
#include <vector>

#include <Engine/Renderer/GPUContracts.hpp>

namespace Physara::Engine
{
    struct FrameData;

    class ClusteredLightGrid final
    {
    public:
        void Build(FrameData &frameData) const;
    };
}