#pragma once

#include <Engine/Renderer/RenderGraph/PassNode.hpp>

namespace Physara::Engine
{
    class RenderGraph;

    class RGBuilder final
    {
    public:
        RGBuilder(RenderGraph &graph, std::uint32_t passIndex);

        RGBuilder &Read(RenderGraphResourceHandle resource);
        RGBuilder &Write(RenderGraphResourceHandle resource);
        RGBuilder &SetExecute(PassNode::ExecuteCallback callback);

    private:
        PassNode &GetPass();

    private:
        RenderGraph *m_Graph{nullptr};
        std::uint32_t m_PassIndex{0};
    };
}