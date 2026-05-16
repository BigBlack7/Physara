#pragma once

#include <Engine/Renderer/Passes/ForwardOpaquePass.hpp>

namespace Physara::Engine
{
    class ForwardTransparentPass final
    {
    public:
        void Execute(const ForwardPassContext &context);
    };
}