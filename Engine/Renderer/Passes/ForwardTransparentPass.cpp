#include "ForwardTransparentPass.hpp"

namespace Physara::Engine
{
    void ForwardTransparentPass::Execute(const ForwardPassContext &context)
    {
        m_ForwardPass.ExecuteTransparent(context);
    }
}