#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    class RHIBuffer
    {
    public:
        virtual ~RHIBuffer() = default;

        virtual std::uint32_t GetSize() const = 0;
        virtual BufferUsageFlags GetUsage() const = 0;

        // 动态Buffer每帧更新数据使用
        virtual void *Map() = 0;
        virtual void Unmap() = 0;
        virtual void UploadData(const void *data, std::uint32_t size, std::uint32_t offset = 0) = 0;
    };
}