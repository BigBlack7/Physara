#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Renderer/RenderGraph/PassNode.hpp>
#include <Engine/Renderer/RenderGraph/RGBuilder.hpp>
#include <Engine/Renderer/RenderGraph/ResourceNode.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHITexture;
}

namespace Physara::Engine
{
    class RenderGraph final
    {
    public:
        void Reset();

        RenderGraphResourceHandle ImportTexture(std::string name, RHI::RHITexture &texture);
        RenderGraphResourceHandle CreateTexture(std::string name, const RHI::RHITextureDesc &desc);
        void SetResourceTexture(RenderGraphResourceHandle handle, RHI::RHITexture *texture);

        RGBuilder AddPass(std::string name);
        void Execute(RHI::RHICommandList &commandList);

        [[nodiscard]] const ResourceNode *GetResource(RenderGraphResourceHandle handle) const;
        [[nodiscard]] ResourceNode *GetResource(RenderGraphResourceHandle handle);
        [[nodiscard]] const std::vector<PassNode> &GetPasses() const { return m_Passes; }
        [[nodiscard]] const std::vector<ResourceNode> &GetResources() const { return m_Resources; }

    private:
        friend class RGBuilder;

        PassNode &GetPass(std::uint32_t index);

    private:
        std::vector<ResourceNode> m_Resources{};
        std::vector<PassNode> m_Passes{};
    };
}