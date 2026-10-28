#include "Upload.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    UploadResourceLease::UploadResourceLease(OpenGLDevice &device, Resource *resource, unsigned long &holds) noexcept
        : m_Device(&device), m_Resource(resource), m_Holds(&holds)
    {
        if (holds++ == 0)
        {
            ++device.m_InternallyRetainedWrappers;
        }
    }

    UploadResourceLease::UploadResourceLease(UploadResourceLease &&other) noexcept
        : m_Device(other.m_Device), m_Resource(std::move(other.m_Resource)),
          m_Holds(std::exchange(other.m_Holds, nullptr))
    {
    }

    UploadResourceLease::~UploadResourceLease()
    {
        if (m_Holds != nullptr && --*m_Holds == 0)
        {
            --m_Device->m_InternallyRetainedWrappers;
        }
    }

    Resource *UploadResourceLease::Get() const noexcept
    {
        return m_Resource.Get();
    }

    bool UploadResourceLease::OnlyInternalOwners() const noexcept
    {
        return m_Resource->GetRefCount() == *m_Holds;
    }

    bool CheckedAlign(std::size_t value, std::size_t alignment, std::size_t &result) noexcept
    {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
            value > std::numeric_limits<std::size_t>::max() - (alignment - 1))
        {
            return false;
        }
        result = (value + alignment - 1) & ~(alignment - 1);
        return true;
    }

    static bool CheckedMultiply(std::size_t a, std::size_t b, std::size_t &result) noexcept
    {
        if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b)
        {
            return false;
        }
        result = a * b;
        return true;
    }

    static bool CheckedFootprint(std::size_t count, std::size_t stride, std::size_t last, std::size_t &result) noexcept
    {
        std::size_t prefix = 0;
        if (!CheckedMultiply(count - 1, stride, prefix) || prefix > std::numeric_limits<std::size_t>::max() - last)
        {
            return false;
        }
        result = prefix + last;
        return true;
    }

    bool BuildUploadLayout(const TextureDesc &desc, std::uint32_t slice, std::uint32_t mip, std::size_t rowPitch,
                           std::size_t depthPitch, TextureUploadLayout &layout) noexcept
    {
        if (mip >= desc.mipLevels || mip >= 32 || desc.sampleCount != 1 ||
            (desc.dimension == TextureDimension::Texture3D ? slice != 0 : slice >= desc.arraySize))
        {
            return false;
        }
        layout.width = std::max(1u, desc.width >> mip);
        layout.height = std::max(1u, desc.height >> mip);
        layout.depth = desc.dimension == TextureDimension::Texture3D ? std::max(1u, desc.depth >> mip) : 1u;
        const auto bytesPerPixel = GetFormatInfo(desc.format).bytesPerBlock;
        if (!CheckedMultiply(layout.width, bytesPerPixel, layout.rowBytes) || layout.rowBytes == 0 ||
            (rowPitch == 0 && layout.height != 1))
        {
            return false;
        }
        layout.rowStride = rowPitch == 0 ? layout.rowBytes : rowPitch;
        std::size_t sliceBytes = 0;
        if (layout.rowStride < layout.rowBytes ||
            !CheckedFootprint(layout.height, layout.rowStride, layout.rowBytes, sliceBytes))
        {
            return false;
        }
        layout.sliceStride = layout.depth == 1 ? sliceBytes : depthPitch;
        std::size_t rows = 0;
        return layout.sliceStride >= sliceBytes &&
               CheckedFootprint(layout.depth, layout.sliceStride, sliceBytes, layout.sourceBytes) &&
               CheckedMultiply(layout.height, layout.depth, rows) &&
               CheckedMultiply(rows, layout.rowBytes, layout.tightBytes);
    }

    void OpenGLDevice::ReportBudget(const char *operation, std::size_t requested, std::size_t retained,
                                    std::size_t limit) const noexcept
    {
        char reason[192]{};
        std::snprintf(reason, sizeof(reason), "CapacityExceeded: requested=%zu retained=%zu limit=%zu", requested,
                      retained, limit);
        Report(operation, reason);
    }

    bool OpenGLDevice::AllocateUpload(UploadWorkPacket &packet, std::size_t size, UploadSlice &slice)
    {
        const auto alignment = std::size_t(m_Caps.minMapAlignment);
        for (auto &chunk : packet.chunks)
        {
            std::size_t offset = 0;
            if (CheckedAlign(chunk->used, alignment, offset) && offset <= chunk->capacity &&
                size <= chunk->capacity - offset && chunk->mapping != nullptr)
            {
                slice = {chunk->name, offset, chunk->mapping + offset};
                chunk->used = offset + size;
                return true;
            }
        }
        std::size_t capacity = 0;
        if (!CheckedAlign(std::max(size, packet.minimumChunk), alignment, capacity) ||
            capacity > UploadCapacityLimit - m_UploadCapacity)
        {
            ReportBudget("AllocateUpload", capacity != 0 ? capacity : size, m_UploadCapacity, UploadCapacityLimit);
            return false;
        }
        try
        {
            auto prepared = std::make_unique<UploadChunk>();
            packet.chunks.push_back(std::move(prepared));
        }
        catch (const std::bad_alloc &)
        {
            Report("AllocateUpload", "CPU allocation failed before native storage");
            return false;
        }
        auto &chunk = *packet.chunks.back();
        if (!BeginNative("AllocateUpload"))
        {
            return false;
        }
        // 节点和预算先归设备, GL 错误也不会把已取得的 name 丢到失败的栈帧之外.
        chunk.capacity = capacity;
        m_UploadCapacity += capacity;
        glCreateBuffers(1, &chunk.name);
        const bool nameCreated = CheckNative("AllocateUpload.Name", chunk.name);
        if (!nameCreated || chunk.name == 0)
        {
            if (nameCreated)
            {
                Report("AllocateUpload.Name", "NativeFailure: zero buffer name");
                m_Phase = Phase::Faulted;
                m_WorkFailed = true;
            }
            packet.invalid = true;
            return false;
        }
        constexpr GLbitfield Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glNamedBufferStorage(chunk.name, GLsizeiptr(capacity), nullptr, Flags);
        if (!CheckNative("AllocateUpload.Storage", chunk.name))
        {
            packet.invalid = true;
            return false;
        }
        chunk.mapping = static_cast<std::byte *>(glMapNamedBufferRange(chunk.name, 0, GLsizeiptr(capacity), Flags));
        const bool mapped = CheckNative("AllocateUpload.Map", chunk.name);
        if (!mapped || chunk.mapping == nullptr)
        {
            if (mapped)
            {
                Report("AllocateUpload.Map", "NativeFailure: persistent mapping unavailable");
            }
            m_Phase = Phase::Faulted;
            m_WorkFailed = true;
            packet.invalid = true;
            return false;
        }
        chunk.used = size;
        slice = {chunk.name, 0, chunk.mapping};
        return true;
    }
}
