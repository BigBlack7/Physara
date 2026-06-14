#include "FrameUploadAllocator.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>

namespace Physara::Engine
{
    void FrameUploadAllocator::Reset()
    {
        m_RetiredFrameBuffers.clear();
        m_Staging.clear();
        m_Offset = 0u;
        m_DirtyStart = 0u;
        m_DirtyEnd = 0u;
        m_HasDirtyRange = false;
    }

    void FrameUploadAllocator::Release()
    {
        m_Buffer.reset();
        m_RetiredFrameBuffers.clear();
        m_Staging.clear();
        m_Capacity = 0u;
        m_Offset = 0u;
        m_DirtyStart = 0u;
        m_DirtyEnd = 0u;
        m_HasDirtyRange = false;
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
            AllocateActiveBuffer(device, size, stats);
            alignedOffset = 0u;
        }
        if (m_Buffer == nullptr)
        {
            return {};
        }

        const std::uint32_t requiredStagingSize = alignedOffset + size;
        if (m_Staging.size() < requiredStagingSize)
        {
            m_Staging.resize(requiredStagingSize);
        }
        std::memcpy(m_Staging.data() + alignedOffset, data, size);
        MarkDirtyRange(alignedOffset, size);
        m_Offset = alignedOffset + size;

        if (stats != nullptr)
        {
            stats->bufferUploadBytes += size;
        }

        return FrameUploadAllocation{m_Buffer.get(), alignedOffset, size};
    }

    void FrameUploadAllocator::Flush(FrameStatistics *stats)
    {
        if (m_Buffer == nullptr || !m_HasDirtyRange || m_DirtyEnd <= m_DirtyStart)
        {
            return;
        }

        const std::uint32_t uploadSize = m_DirtyEnd - m_DirtyStart;
        m_Buffer->UploadData(m_Staging.data() + m_DirtyStart, uploadSize, m_DirtyStart);
        if (stats != nullptr)
        {
            ++stats->bufferUploadChunks;
        }
        m_DirtyStart = 0u;
        m_DirtyEnd = 0u;
        m_HasDirtyRange = false;
    }

    void FrameUploadAllocator::AllocateActiveBuffer(RHI::RHIDevice &device, std::uint32_t requiredBytes, FrameStatistics *stats)
    {
        Flush(stats);
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
        m_Staging.clear();
        m_DirtyStart = 0u;
        m_DirtyEnd = 0u;
        m_HasDirtyRange = false;
    }

    void FrameUploadAllocator::MarkDirtyRange(std::uint32_t offset, std::uint32_t size)
    {
        const std::uint32_t end = offset + size;
        if (!m_HasDirtyRange)
        {
            m_DirtyStart = offset;
            m_DirtyEnd = end;
            m_HasDirtyRange = true;
            return;
        }

        m_DirtyStart = std::min(m_DirtyStart, offset);
        m_DirtyEnd = std::max(m_DirtyEnd, end);
    }

    std::uint32_t FrameUploadAllocator::AlignUp(std::uint32_t value, std::uint32_t alignment)
    {
        return alignment == 0u ? value : ((value + alignment - 1u) / alignment) * alignment;
    }
}
