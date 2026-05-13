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

        RGBuilder AddPass(std::string name);
        void Execute(RHI::RHICommandList &commandList);

        [[nodiscard]] const ResourceNode *GetResource(RenderGraphResourceHandle handle) const;
        [[nodiscard]] ResourceNode *GetResource(RenderGraphResourceHandle handle);

    private:
        friend class RGBuilder;

        PassNode &GetPass(std::uint32_t index);

    private:
        std::vector<ResourceNode> m_Resources{};
        std::vector<PassNode> m_Passes{};
    };
}