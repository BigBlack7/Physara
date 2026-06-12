#include "FrameUploadAllocator.hpp"

#include <algorithm>
#include <cassert>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>

namespace Physara::Engine
{
    void FrameUploadAllocator::Reset()
    {
        m_RetiredFrameBuffers.clear();
        m_Offset = 0u;
    }

    void FrameUploadAllocator::Release()
    {
        m_Buffer.reset();
        m_RetiredFrameBuffers.clear();
        m_Capacity = 0u;
        m_Offset = 0u;
    }

    FrameUploadAllocation FrameUploadAllocator::Upload(
        RHI::RHIDevice &device,
        const void *data,
        std::uint32_t size,
        FrameStatistics *stats)
    {
        assert(data != nullptr);
        if (data == nullptr || size == 0u)
        {
            return {};
        }

        std::uint32_t alignedOffset = AlignUp(m_Offset, kAlignment);
        if (m_Buffer == nullptr || alignedOffset + size > m_Capacity)
        {
            AllocateActiveBuffer(device, size);
            alignedOffset = 0u;
        }
        if (m_Buffer == nullptr)
        {
            return {};
        }

        m_Buffer->UploadData(data, size, alignedOffset);
        m_Offset = alignedOffset + size;

        if (stats != nullptr)
        {
            stats->bufferUploadBytes += size;
            ++stats->bufferUploadChunks;
        }

        return FrameUploadAllocation{m_Buffer.get(), alignedOffset, size};
    }

    void FrameUploadAllocator::AllocateActiveBuffer(RHI::RHIDevice &device, std::uint32_t requiredBytes)
    {
        const std::uint32_t newCapacity = std::max(AlignUp(requiredBytes, kAlignment), std::max(m_Capacity * 2u, kDefaultCapacity));

        RHI::RHIBufferDesc desc{};
        desc.size = newCapacity;
        desc.usage = RHI::BufferUsage::Uniform | RHI::BufferUsage::Storage;
        desc.dynamic = true;
        std::unique_ptr<RHI::RHIBuffer> newBuffer = device.CreateBuffer(desc);
        if (newBuffer == nullptr)
        {
            return;
        }

        if (m_Buffer != nullptr)
        {
            m_RetiredFrameBuffers.push_back(std::move(m_Buffer));
        }
        m_Buffer = std::move(newBuffer);
        m_Capacity = newCapacity;
        m_Offset = 0u;
    }

    std::uint32_t FrameUploadAllocator::AlignUp(std::uint32_t value, std::uint32_t alignment)
    {
        return alignment == 0u ? value : ((value + alignment - 1u) / alignment) * alignment;
    }
}