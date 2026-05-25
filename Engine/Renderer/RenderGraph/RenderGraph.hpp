#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Engine/Renderer/RenderGraph/PassNode.hpp>
#include <Engine/Renderer/RenderGraph/RGBuilder.hpp>
#include <Engine/Renderer/RenderGraph/ResourceNode.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
    class RHITexture;
}

namespace Physara::Engine::RenderGraphDetail
{
    struct CompiledGraph;
}

namespace Physara::Engine
{
    class RenderGraph final
    {
    public:
        void Reset();

        RenderGraphResourceHandle ImportTexture(std::string name, RHI::RHITexture &texture);
        RenderGraphResourceHandle CreateTexture(std::string name, const RHI::RHITextureDesc &desc);
        void MarkOutput(RenderGraphResourceHandle resource);

        RGBuilder AddPass(std::string name);
        void Execute(RHI::RHICommandList &commandList, RHI::RHIDevice *device = nullptr);
        void ReleasePooledResources();

        [[nodiscard]] const ResourceNode *GetResource(RenderGraphResourceHandle handle) const;
        [[nodiscard]] ResourceNode *GetResource(RenderGraphResourceHandle handle);

    private:
        friend class RGBuilder;

        PassNode &GetPass(std::uint32_t index);
        [[nodiscard]] RenderGraphDetail::CompiledGraph Compile() const;

    private:
        struct PooledTexture
        {
            RHI::RHITextureDesc desc{};
            std::unique_ptr<RHI::RHITexture> texture{};
        };

        [[nodiscard]] std::unique_ptr<RHI::RHITexture> AcquireTexture(RHI::RHIDevice &device, const RHI::RHITextureDesc &desc);
        void ReleaseTexture(const RHI::RHITextureDesc &desc, std::unique_ptr<RHI::RHITexture> texture);

        std::vector<ResourceNode> m_Resources{};
        std::vector<PassNode> m_Passes{};
        std::vector<PooledTexture> m_TexturePool{};
    };
}