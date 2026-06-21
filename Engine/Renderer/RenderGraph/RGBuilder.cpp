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
        return ReadTexture(resource);
    }

    RGBuilder &RGBuilder::Write(RenderGraphResourceHandle resource)
    {
        return WriteAttachment(resource);
    }

    RGBuilder &RGBuilder::ReadTexture(RenderGraphResourceHandle resource, RHI::ShaderStageFlags stages)
    {
        return Read(resource, RHI::ResourceState::ShaderResource, stages, RHI::ResourceAccess::ShaderRead);
    }

    RGBuilder &RGBuilder::WriteAttachment(RenderGraphResourceHandle resource)
    {
        if (IsDepthAttachment(resource))
        {
            return Write(
                resource,
                RHI::ResourceState::DepthWrite,
                RHI::ShaderStageBit::Fragment,
                RHI::ResourceAccess::DepthStencilWrite);
        }

        return Write(
            resource,
            RHI::ResourceState::RenderTarget,
            RHI::ShaderStageBit::Fragment,
            RHI::ResourceAccess::ColorAttachmentWrite);
    }

    RGBuilder &RGBuilder::ReadTransfer(RenderGraphResourceHandle resource)
    {
        return Read(
            resource,
            RHI::ResourceState::CopySource,
            RHI::ShaderStageBit::None,
            RHI::ResourceAccess::TransferRead);
    }

    RGBuilder &RGBuilder::WriteTransfer(RenderGraphResourceHandle resource)
    {
        return Write(
            resource,
            RHI::ResourceState::CopyDest,
            RHI::ShaderStageBit::None,
            RHI::ResourceAccess::TransferWrite);
    }

    RGBuilder &RGBuilder::Read(
        RenderGraphResourceHandle resource,
        RHI::ResourceState state,
        RHI::ShaderStageFlags stages,
        RHI::ResourceAccessFlags access)
    {
        GetPass().AddAccess(resource, RenderGraphResourceUsage::Read, state, stages, access);
        return *this;
    }

    RGBuilder &RGBuilder::Write(
        RenderGraphResourceHandle resource,
        RHI::ResourceState state,
        RHI::ShaderStageFlags stages,
        RHI::ResourceAccessFlags access)
    {
        GetPass().AddAccess(resource, RenderGraphResourceUsage::Write, state, stages, access);
        return *this;
    }

    RGBuilder &RGBuilder::SetSideEffect(bool sideEffect)
    {
        GetPass().SetSideEffect(sideEffect);
        return *this;
    }

    RGBuilder &RGBuilder::SetGPUTimingScope(std::uint32_t scope)
    {
        GetPass().SetGPUTimingScope(scope);
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

    bool RGBuilder::IsDepthAttachment(RenderGraphResourceHandle resource) const
    {
        const ResourceNode *node = m_Graph != nullptr ? m_Graph->GetResource(resource) : nullptr;
        if (node == nullptr)
        {
            return false;
        }

        const RHI::TextureFormat format = node->GetTextureDesc().format;
        return format == RHI::TextureFormat::Depth24Stencil8 || format == RHI::TextureFormat::Depth32F;
    }
}
