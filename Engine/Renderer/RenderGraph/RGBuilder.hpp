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
        RGBuilder &ReadTexture(RenderGraphResourceHandle resource, RHI::ShaderStageFlags stages = RHI::ShaderStageBit::Fragment);
        RGBuilder &WriteAttachment(RenderGraphResourceHandle resource);
        RGBuilder &ReadTransfer(RenderGraphResourceHandle resource);
        RGBuilder &WriteTransfer(RenderGraphResourceHandle resource);
        RGBuilder &Read(
            RenderGraphResourceHandle resource,
            RHI::ResourceState state,
            RHI::ShaderStageFlags stages,
            RHI::ResourceAccessFlags access);
        RGBuilder &Write(
            RenderGraphResourceHandle resource,
            RHI::ResourceState state,
            RHI::ShaderStageFlags stages,
            RHI::ResourceAccessFlags access);
        RGBuilder &SetSideEffect(bool sideEffect = true);
        RGBuilder &SetExecute(PassNode::ExecuteCallback callback);

    private:
        PassNode &GetPass();
        [[nodiscard]] bool IsDepthAttachment(RenderGraphResourceHandle resource) const;

    private:
        RenderGraph *m_Graph{nullptr};
        std::uint32_t m_PassIndex{0};
    };
}