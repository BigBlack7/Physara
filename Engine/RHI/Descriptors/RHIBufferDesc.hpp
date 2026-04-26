#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    struct RHIBufferDesc
    {
        std::uint32_t size = 0;                       // 字节大小
        BufferUsageFlags usage = BufferUsage::Vertex; // Vertex/Index/Uniform/Storage, 可按位组合
        bool dynamic = false;                         // true = 高频更新, false = 静态上传
        const void *initialData = nullptr;            // 可选初始数据
    };
}