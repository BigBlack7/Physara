#include "RenderGraph.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::Engine::RenderGraphDetail
{
    constexpr std::uint32_t kInvalidPass = std::numeric_limits<std::uint32_t>::max();

    struct ResourceLifetime
    {
        std::uint32_t firstUse{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t lastUse{0};
        bool used{false};
    };

    struct CompiledGraph
    {
        std::vector<std::uint32_t> order{};
        std::vector<ResourceLifetime> lifetimes{};
    };

    struct CombinedAccess
    {
        RenderGraphResourceHandle resource{};
        bool read{false};
        bool write{false};
    };

    struct TrackedState
    {
        RHI::ResourceState state{RHI::ResourceState::Undefined};
        RHI::ShaderStageFlags stages{RHI::ShaderStageBit::None};
        RHI::ResourceAccessFlags access{RHI::ResourceAccess::None};
        bool initialized{false};
    };

    [[nodiscard]] bool IsDepthFormat(RHI::TextureFormat format)
    {
        return format == RHI::TextureFormat::Depth24Stencil8 || format == RHI::TextureFormat::Depth32F;
    }

    [[nodiscard]] bool TextureDescMatches(const RHI::RHITextureDesc &lhs, const RHI::RHITextureDesc &rhs)
    {
        return lhs.width == rhs.width &&
               lhs.height == rhs.height &&
               lhs.depth == rhs.depth &&
               lhs.mipLevels == rhs.mipLevels &&
               lhs.arrayLayers == rhs.arrayLayers &&
               lhs.samples == rhs.samples &&
               lhs.format == rhs.format &&
               lhs.dimension == rhs.dimension &&
               lhs.usage == rhs.usage;
    }

    [[nodiscard]] std::vector<CombinedAccess> CombineAccesses(const PassNode &pass)
    {
        std::vector<CombinedAccess> combined;
        for (const RenderGraphResourceAccess &access : pass.GetResourceAccesses())
        {
            if (!access.resource.IsValid())
            {
                continue;
            }

            auto it = std::find_if(
                combined.begin(),
                combined.end(),
                [&](const CombinedAccess &candidate)
                {
                    return candidate.resource == access.resource;
                });

            if (it == combined.end())
            {
                it = combined.insert(combined.end(), CombinedAccess{access.resource});
            }

            if (access.usage == RenderGraphResourceUsage::Read)
            {
                it->read = true;
            }
            else
            {
                it->write = true;
            }
        }
        return combined;
    }

    void AddEdge(std::vector<std::vector<std::uint32_t>> &edges, std::uint32_t from, std::uint32_t to)
    {
        if (from == to || from == kInvalidPass || to == kInvalidPass)
        {
            return;
        }

        std::vector<std::uint32_t> &outgoing = edges[from];
        if (std::find(outgoing.begin(), outgoing.end(), to) == outgoing.end())
        {
            outgoing.push_back(to);
        }
    }

    [[nodiscard]] RHI::RHIResourceBarrier MakeBarrier(
        const ResourceNode &resource,
        bool write,
        const TrackedState &before)
    {
        const bool depth = IsDepthFormat(resource.GetTextureDesc().format);

        RHI::RHIResourceBarrier barrier{};
        barrier.before = before.initialized ? before.state : RHI::ResourceState::Common;
        barrier.srcStages = before.stages;
        barrier.srcAccess = before.access;
        barrier.dstStages = RHI::ShaderStageBit::Fragment;

        if (write)
        {
            barrier.after = depth ? RHI::ResourceState::DepthWrite : RHI::ResourceState::RenderTarget;
            barrier.dstAccess = depth ? RHI::ResourceAccess::DepthStencilWrite : RHI::ResourceAccess::ColorAttachmentWrite;
        }
        else
        {
            barrier.after = RHI::ResourceState::ShaderResource;
            barrier.dstAccess = RHI::ResourceAccess::ShaderRead;
        }

        return barrier;
    }

    [[nodiscard]] bool NeedsBarrier(const TrackedState &before, const RHI::RHIResourceBarrier &barrier, bool write)
    {
        if (!before.initialized)
        {
            return !write;
        }

        return before.state != barrier.after || before.access != barrier.dstAccess;
    }
}

namespace Physara::Engine
{
    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_Resources.clear();
    }

    void RenderGraph::ReleasePooledResources()
    {
        m_TexturePool.clear();
    }

    RenderGraphResourceHandle RenderGraph::ImportTexture(std::string name, RHI::RHITexture &texture)
    {
        RHI::RHITextureDesc desc{};
        desc.width = texture.GetWidth();
        desc.height = texture.GetHeight();
        desc.depth = 1;
        desc.format = texture.GetFormat();
        desc.dimension = texture.GetDimension();
        desc.usage = texture.GetUsage();
        desc.mipLevels = texture.GetMipLevels();
        desc.arrayLayers = texture.GetArrayLayers();
        desc.samples = 1;

        const std::uint32_t index = static_cast<std::uint32_t>(m_Resources.size());
        m_Resources.emplace_back(std::move(name), desc, &texture, true);
        return {index};
    }

    RenderGraphResourceHandle RenderGraph::CreateTexture(std::string name, const RHI::RHITextureDesc &desc)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(m_Resources.size());
        m_Resources.emplace_back(std::move(name), desc, nullptr, false);
        return {index};
    }

    void RenderGraph::MarkOutput(RenderGraphResourceHandle resource)
    {
        if (ResourceNode *node = GetResource(resource))
        {
            node->SetOutput(true);
        }
    }

    RGBuilder RenderGraph::AddPass(std::string name)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(m_Passes.size());
        m_Passes.emplace_back(std::move(name));
        return RGBuilder(*this, index);
    }

    RenderGraphDetail::CompiledGraph RenderGraph::Compile() const
    {
        using namespace RenderGraphDetail;

        const std::uint32_t passCount = static_cast<std::uint32_t>(m_Passes.size());
        const std::uint32_t resourceCount = static_cast<std::uint32_t>(m_Resources.size());

        std::vector<std::vector<std::uint32_t>> edges(passCount);
        std::vector<std::uint32_t> lastWriter(resourceCount, kInvalidPass);
        std::vector<std::vector<std::uint32_t>> readersSinceWrite(resourceCount);

        for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
        {
            for (const CombinedAccess &access : CombineAccesses(m_Passes[passIndex]))
            {
                if (access.resource.index >= resourceCount)
                {
                    continue;
                }

                const std::uint32_t resourceIndex = access.resource.index;
                if (access.read && lastWriter[resourceIndex] != kInvalidPass)
                {
                    AddEdge(edges, lastWriter[resourceIndex], passIndex);
                }

                if (access.write)
                {
                    if (lastWriter[resourceIndex] != kInvalidPass)
                    {
                        AddEdge(edges, lastWriter[resourceIndex], passIndex);
                    }

                    for (std::uint32_t reader : readersSinceWrite[resourceIndex])
                    {
                        AddEdge(edges, reader, passIndex);
                    }
                    readersSinceWrite[resourceIndex].clear();
                    lastWriter[resourceIndex] = passIndex;
                }

                if (access.read)
                {
                    readersSinceWrite[resourceIndex].push_back(passIndex);
                }
            }
        }

        std::vector<bool> live(passCount, false);
        std::vector<std::vector<std::uint32_t>> reverseEdges(passCount);
        for (std::uint32_t from = 0; from < passCount; ++from)
        {
            for (std::uint32_t to : edges[from])
            {
                reverseEdges[to].push_back(from);
            }
        }

        std::vector<std::uint32_t> stack;
        for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
        {
            bool root = m_Passes[passIndex].HasSideEffect();
            for (const CombinedAccess &access : CombineAccesses(m_Passes[passIndex]))
            {
                if (access.write && access.resource.index < resourceCount && m_Resources[access.resource.index].IsOutput())
                {
                    root = true;
                    break;
                }
            }

            if (root)
            {
                live[passIndex] = true;
                stack.push_back(passIndex);
            }
        }

        while (!stack.empty())
        {
            const std::uint32_t passIndex = stack.back();
            stack.pop_back();
            for (std::uint32_t dependency : reverseEdges[passIndex])
            {
                if (!live[dependency])
                {
                    live[dependency] = true;
                    stack.push_back(dependency);
                }
            }
        }

        std::vector<std::uint32_t> incoming(passCount, 0);
        for (std::uint32_t from = 0; from < passCount; ++from)
        {
            if (!live[from])
            {
                continue;
            }

            for (std::uint32_t to : edges[from])
            {
                if (live[to])
                {
                    ++incoming[to];
                }
            }
        }

        CompiledGraph compiled{};
        compiled.lifetimes.resize(resourceCount);

        std::vector<bool> emitted(passCount, false);
        for (std::uint32_t emittedCount = 0; emittedCount < passCount;)
        {
            bool progressed = false;
            for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
            {
                if (!live[passIndex] || emitted[passIndex] || incoming[passIndex] != 0)
                {
                    continue;
                }

                emitted[passIndex] = true;
                ++emittedCount;
                progressed = true;
                compiled.order.push_back(passIndex);

                for (std::uint32_t to : edges[passIndex])
                {
                    if (live[to] && incoming[to] > 0)
                    {
                        --incoming[to];
                    }
                }
            }

            if (!progressed)
            {
                const auto liveCount = static_cast<std::uint32_t>(std::count(live.begin(), live.end(), true));
                if (compiled.order.size() != liveCount)
                {
                    PHYSARA_CORE_ERROR("RenderGraph cycle detected; falling back to declaration order for remaining live passes.");
                    for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
                    {
                        if (live[passIndex] && !emitted[passIndex])
                        {
                            emitted[passIndex] = true;
                            compiled.order.push_back(passIndex);
                        }
                    }
                }
                break;
            }

            if (static_cast<std::uint32_t>(compiled.order.size()) == std::count(live.begin(), live.end(), true))
            {
                break;
            }
        }

        for (std::uint32_t orderIndex = 0; orderIndex < compiled.order.size(); ++orderIndex)
        {
            const PassNode &pass = m_Passes[compiled.order[orderIndex]];
            for (const CombinedAccess &access : CombineAccesses(pass))
            {
                if (access.resource.index >= resourceCount)
                {
                    continue;
                }

                ResourceLifetime &lifetime = compiled.lifetimes[access.resource.index];
                lifetime.used = true;
                lifetime.firstUse = std::min(lifetime.firstUse, orderIndex);
                lifetime.lastUse = std::max(lifetime.lastUse, orderIndex);
            }
        }

        return compiled;
    }

    void RenderGraph::Execute(RHI::RHICommandList &commandList, RHI::RHIDevice *device)
    {
        using namespace RenderGraphDetail;

        CompiledGraph compiled = Compile();
        RenderGraphContext context{commandList, *this};

        for (std::uint32_t resourceIndex = 0; resourceIndex < m_Resources.size(); ++resourceIndex)
        {
            ResourceNode &resource = m_Resources[resourceIndex];
            if (!resource.IsImported() && compiled.lifetimes[resourceIndex].used)
            {
                if (device == nullptr)
                {
                    PHYSARA_CORE_ERROR("RenderGraph virtual resource '{}' needs a device for allocation.", resource.GetName());
                    return;
                }

                resource.AcquireOwnedTexture(AcquireTexture(*device, resource.GetTextureDesc()));
            }
        }

        std::vector<TrackedState> states(m_Resources.size());
        for (std::uint32_t orderIndex = 0; orderIndex < compiled.order.size(); ++orderIndex)
        {
            const std::uint32_t passIndex = compiled.order[orderIndex];
            const PassNode &pass = m_Passes[passIndex];

            for (const CombinedAccess &access : CombineAccesses(pass))
            {
                if (access.resource.index >= m_Resources.size())
                {
                    continue;
                }

                ResourceNode &resource = m_Resources[access.resource.index];
                RHI::RHITexture *texture = resource.GetTexture();
                if (texture == nullptr)
                {
                    PHYSARA_CORE_ERROR("RenderGraph resource '{}' has no backing texture.", resource.GetName());
                    continue;
                }

                const bool write = access.write;
                TrackedState &state = states[access.resource.index];
                const RHI::RHIResourceBarrier barrier = MakeBarrier(resource, write, state);
                if (NeedsBarrier(state, barrier, write))
                {
                    commandList.TextureBarrier(texture, barrier);
                }

                state.initialized = true;
                state.state = barrier.after;
                state.stages = barrier.dstStages;
                state.access = barrier.dstAccess;
            }

            commandList.BeginDebugLabel(pass.GetName().c_str());
            pass.Execute(context);
            commandList.EndDebugLabel();

            for (std::uint32_t resourceIndex = 0; resourceIndex < m_Resources.size(); ++resourceIndex)
            {
                ResourceNode &resource = m_Resources[resourceIndex];
                if (!resource.IsImported() &&
                    compiled.lifetimes[resourceIndex].used &&
                    compiled.lifetimes[resourceIndex].lastUse == orderIndex)
                {
                    ReleaseTexture(resource.GetTextureDesc(), resource.ReleaseOwnedTexture());
                }
            }
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

    std::unique_ptr<RHI::RHITexture> RenderGraph::AcquireTexture(RHI::RHIDevice &device, const RHI::RHITextureDesc &desc)
    {
        auto it = std::find_if(
            m_TexturePool.begin(),
            m_TexturePool.end(),
            [&](const PooledTexture &pooled)
            {
                return RenderGraphDetail::TextureDescMatches(pooled.desc, desc);
            });

        if (it != m_TexturePool.end())
        {
            std::unique_ptr<RHI::RHITexture> texture = std::move(it->texture);
            m_TexturePool.erase(it);
            return texture;
        }

        return device.CreateTexture(desc);
    }

    void RenderGraph::ReleaseTexture(const RHI::RHITextureDesc &desc, std::unique_ptr<RHI::RHITexture> texture)
    {
        if (texture)
        {
            m_TexturePool.push_back({desc, std::move(texture)});
        }
    }
}