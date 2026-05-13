#include "PassNode.hpp"

#include <utility>

namespace Physara::Engine
{
    PassNode::PassNode(std::string name)
        : m_Name(std::move(name))
    {
    }

    void PassNode::AddRead(RenderGraphResourceHandle resource)
    {
        m_ResourceAccesses.push_back({resource, RenderGraphResourceUsage::Read});
    }

    void PassNode::AddWrite(RenderGraphResourceHandle resource)
    {
        m_ResourceAccesses.push_back({resource, RenderGraphResourceUsage::Write});
    }

    void PassNode::SetExecuteCallback(ExecuteCallback callback)
    {
        m_ExecuteCallback = std::move(callback);
    }

    void PassNode::Execute(RenderGraphContext &context) const
    {
        if (m_ExecuteCallback)
        {
            m_ExecuteCallback(context);
        }
    }
}