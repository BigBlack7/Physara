#include "ForwardTransparentPass.hpp"

namespace Physara::Engine
{
    void ForwardTransparentPass::Execute(const ForwardPassContext &)
    {
        // Transparent drawing will share the Forward shader once Mesh GPU buffers and per-material binding exist.
    }
}