#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Renderer/RenderGraph/ResourceNode.hpp>

namespace Physara::RHI
{
    class RHICommandList;
}

namespace Physara::Engine
{
    class RenderGraph;

    enum class RenderGraphResourceUsage
    {
        Read,
        Write
    };

    struct RenderGraphResourceAccess
    {
        RenderGraphResourceHandle resource{};
        RenderGraphResourceUsage usage{RenderGraphResourceUsage::Read};
    };

    struct RenderGraphContext
    {
        RHI::RHICommandList &commandList;
        RenderGraph &graph;
    };

    class PassNode final
    {
    public:
        using ExecuteCallback = std::function<void(RenderGraphContext &)>;

        explicit PassNode(std::string name);

        [[nodiscard]] const std::string &GetName() const { return m_Name; }
        [[nodiscard]] const std::vector<RenderGraphResourceAccess> &GetResourceAccesses() const { return m_ResourceAccesses; }

        void AddRead(RenderGraphResourceHandle resource);
        void AddWrite(RenderGraphResourceHandle resource);
        void SetExecuteCallback(ExecuteCallback callback);
        void Execute(RenderGraphContext &context) const;

    private:
        std::string m_Name{};
        std::vector<RenderGraphResourceAccess> m_ResourceAccesses{};
        ExecuteCallback m_ExecuteCallback{};
    };

    inline PassNode::PassNode(std::string name)
        : m_Name(std::move(name))
    {
    }

    inline void PassNode::AddRead(RenderGraphResourceHandle resource)
    {
        m_ResourceAccesses.push_back({resource, RenderGraphResourceUsage::Read});
    }

    inline void PassNode::AddWrite(RenderGraphResourceHandle resource)
    {
        m_ResourceAccesses.push_back({resource, RenderGraphResourceUsage::Write});
    }

    inline void PassNode::SetExecuteCallback(ExecuteCallback callback)
    {
        m_ExecuteCallback = std::move(callback);
    }

    inline void PassNode::Execute(RenderGraphContext &context) const
    {
        if (m_ExecuteCallback)
        {
            m_ExecuteCallback(context);
        }
    }
}