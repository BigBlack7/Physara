#include "RenderGraph.hpp"

#include <utility>

#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::Engine
{
    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_Resources.clear();
    }

    RenderGraphResourceHandle RenderGraph::ImportTexture(std::string name, RHI::RHITexture &texture)
    {
        RHI::RHITextureDesc desc{};
        desc.width = texture.GetWidth();
        desc.height = texture.GetHeight();
        desc.format = texture.GetFormat();
        desc.dimension = texture.GetDimension();
        desc.usage = texture.GetUsage();
        desc.mipLevels = texture.GetMipLevels();
        desc.arrayLayers = texture.GetArrayLayers();
        desc.samples = 1;

        const std::uint32_t index = static_cast<std::uint32_t>(m_Resources.size());
        m_Resources.emplace_back(std::move(name), desc, &texture);
        return {index};
    }

    RGBuilder RenderGraph::AddPass(std::string name)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(m_Passes.size());
        m_Passes.emplace_back(std::move(name));
        return RGBuilder(*this, index);
    }

    void RenderGraph::Execute(RHI::RHICommandList &commandList)
    {
        RenderGraphContext context{commandList, *this};
        for (const PassNode &pass : m_Passes)
        {
            commandList.BeginDebugLabel(pass.GetName().c_str());
            pass.Execute(context);
            commandList.EndDebugLabel();
        }
    }

    const ResourceNode *RenderGraph::GetResource(RenderGraphResourceHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= m_Resources.size())
        {
            return nullptr;
        }

        return &m_Resources[handle.index];
    }

    ResourceNode *RenderGraph::GetResource(RenderGraphResourceHandle handle)
    {
        if (!handle.IsValid() || handle.index >= m_Resources.size())
        {
            return nullptr;
        }

        return &m_Resources[handle.index];
    }

    PassNode &RenderGraph::GetPass(std::uint32_t index)
    {
        return m_Passes[index];
    }
}