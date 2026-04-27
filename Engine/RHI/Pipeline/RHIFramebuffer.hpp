#pragma once

#include <cstdint>
#include <vector>

#include <Engine/RHI/RHIDefinitions.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>

namespace Physara::RHI
{
    class RHITexture;

    struct RHIFramebufferDesc
    {
        std::vector<RHITexture *> colorAttachments; // 对应RenderPassDesc 颜色附件槽
        RHITexture *depthAttachment = nullptr;

        std::uint32_t width = 0;
        std::uint32_t height = 0;

        // Vulkan创建Framebuffer时需要兼容的RenderPass描述
        const RHIRenderPassDesc *renderPassDesc = nullptr;
    };

    class RHIFramebuffer
    {
    public:
        virtual ~RHIFramebuffer() = default;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;
    };
}