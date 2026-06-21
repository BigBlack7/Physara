#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Renderer/RenderGraph/ResourceNode.hpp>
#include <Engine/RHI/RHIDefinitions.hpp>

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
        RHI::ResourceState state{RHI::ResourceState::ShaderResource};
        RHI::ShaderStageFlags stages{RHI::ShaderStageBit::Fragment};
        RHI::ResourceAccessFlags access{RHI::ResourceAccess::ShaderRead};
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
        [[nodiscard]] bool HasSideEffect() const { return m_SideEffect; }
        [[nodiscard]] bool HasGPUTimingScope() const { return m_GPUTimingScope != InvalidGPUTimingScope; }
        [[nodiscard]] std::uint32_t GetGPUTimingScope() const { return m_GPUTimingScope; }

        void AddRead(RenderGraphResourceHandle resource);
        void AddWrite(RenderGraphResourceHandle resource);
        void AddAccess(
            RenderGraphResourceHandle resource,
            RenderGraphResourceUsage usage,
            RHI::ResourceState state,
            RHI::ShaderStageFlags stages,
            RHI::ResourceAccessFlags access);
        void SetSideEffect(bool sideEffect);
        void SetGPUTimingScope(std::uint32_t scope);
        void SetExecuteCallback(ExecuteCallback callback);
        void Execute(RenderGraphContext &context) const;

    private:
        std::string m_Name{};
        std::vector<RenderGraphResourceAccess> m_ResourceAccesses{};
        ExecuteCallback m_ExecuteCallback{};
        bool m_SideEffect{false};
        static constexpr std::uint32_t InvalidGPUTimingScope = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t m_GPUTimingScope{InvalidGPUTimingScope};
    };

    inline PassNode::PassNode(std::string name)
        : m_Name(std::move(name))
    {
    }

    inline void PassNode::AddRead(RenderGraphResourceHandle resource)
    {
        AddAccess(
            resource,
            RenderGraphResourceUsage::Read,
            RHI::ResourceState::ShaderResource,
            RHI::ShaderStageBit::Fragment,
            RHI::ResourceAccess::ShaderRead);
    }

    inline void PassNode::AddWrite(RenderGraphResourceHandle resource)
    {
        AddAccess(
            resource,
            RenderGraphResourceUsage::Write,
            RHI::ResourceState::RenderTarget,
            RHI::ShaderStageBit::Fragment,
            RHI::ResourceAccess::ColorAttachmentWrite);
    }

    inline void PassNode::AddAccess(
        RenderGraphResourceHandle resource,
        RenderGraphResourceUsage usage,
        RHI::ResourceState state,
        RHI::ShaderStageFlags stages,
        RHI::ResourceAccessFlags access)
    {
        m_ResourceAccesses.push_back({resource, usage, state, stages, access});
    }

    inline void PassNode::SetSideEffect(bool sideEffect)
    {
        m_SideEffect = sideEffect;
    }

    inline void PassNode::SetGPUTimingScope(std::uint32_t scope)
    {
        m_GPUTimingScope = scope;
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
