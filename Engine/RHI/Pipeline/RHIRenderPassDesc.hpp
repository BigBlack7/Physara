#pragma once

#include <cstdint>
#include <vector>

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    struct RHIAttachmentDesc
    {
        TextureFormat format = TextureFormat::RGBA8;
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;
        std::uint32_t samples = 1; // MSAA
    };

    struct RHIRenderPassDesc
    {
        // 最多4颜色附件(例如G-Buffer)
        std::vector<RHIAttachmentDesc> colorAttachments;

        // 默认深度格式设置为Depth24Stencil8, 避免启用深度时遗漏格式设置
        RHIAttachmentDesc depthAttachment{
            TextureFormat::Depth24Stencil8,
            LoadOp::Clear,
            StoreOp::Store,
            1u
        };

        bool hasDepth = false; // 是否启用深度附件
    };
}