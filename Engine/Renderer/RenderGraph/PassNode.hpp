#pragma once

#include <functional>
#include <string>
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
        [[nodiscard]] bool HasExecuteCallback() const { return static_cast<bool>(m_ExecuteCallback); }

        void AddRead(RenderGraphResourceHandle resource);
        void AddWrite(RenderGraphResourceHandle resource);
        void SetExecuteCallback(ExecuteCallback callback);
        void Execute(RenderGraphContext &context) const;

    private:
        std::string m_Name{};
        std::vector<RenderGraphResourceAccess> m_ResourceAccesses{};
        ExecuteCallback m_ExecuteCallback{};
    };
}