#include "RGBuilder.hpp"

#include <Engine/Renderer/RenderGraph/RenderGraph.hpp>

namespace Physara::Engine
{
    RGBuilder::RGBuilder(RenderGraph &graph, std::uint32_t passIndex)
        : m_Graph(&graph),
          m_PassIndex(passIndex)
    {
    }

    RGBuilder &RGBuilder::Read(RenderGraphResourceHandle resource)
    {
        GetPass().AddRead(resource);
        return *this;
    }

    RGBuilder &RGBuilder::Write(RenderGraphResourceHandle resource)
    {
        GetPass().AddWrite(resource);
        return *this;
    }

    RGBuilder &RGBuilder::SetExecute(PassNode::ExecuteCallback callback)
    {
        GetPass().SetExecuteCallback(std::move(callback));
        return *this;
    }

    PassNode &RGBuilder::GetPass()
    {
        return m_Graph->GetPass(m_PassIndex);
    }
}