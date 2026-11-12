#include "Upload.hpp"

/// @file
/// @brief 维护包的资源保活和上传源布局, 为即时写入取得有界 coherent 区段.
/// 准入与实际分配使用同一最小 chunk 和对齐规则, 已分配容量沿物理包退休.

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    UploadResourceLease::UploadResourceLease(OpenGLDevice &device, Resource *resource, InternalOwnership &owners,
                                             std::vector<InternalOwnership *> dependencies)
        : m_Device(&device), m_Resource(resource), m_Owners(&owners), m_Dependencies(std::move(dependencies))
    {
        if (owners.packetHolds++ == 0)
        {
            ++device.m_InternallyRetainedWrappers;
            // 复合对象本身只有一组子对象强引用, 跨多个包保活时不能重复计算这组边.
            for (auto *dependency : m_Dependencies)
            {
                ++dependency->dependencyHolds;
            }
        }
    }

    UploadResourceLease::UploadResourceLease(UploadResourceLease &&other) noexcept
        : m_Device(other.m_Device), m_Resource(std::move(other.m_Resource)),
          m_Owners(std::exchange(other.m_Owners, nullptr)), m_Dependencies(std::move(other.m_Dependencies))
    {
    }

    UploadResourceLease::~UploadResourceLease()
    {
        // 先撤销内部记账, 随后 m_Resource 的成员析构才 Release wrapper, 防止最后引用释放时仍被计作内部 owner.
        // 每包都保存父对象的相同依赖边, 但只有最后一个包退出才撤销边, 不依赖多个包的退休先后.
        if (m_Owners != nullptr && --m_Owners->packetHolds == 0)
        {
            --m_Device->m_InternallyRetainedWrappers;
            for (auto *dependency : m_Dependencies)
            {
                --dependency->dependencyHolds;
            }
        }
    }

    Resource *UploadResourceLease::Get() const noexcept
    {
        return m_Resource.Get();
    }

    bool UploadResourceLease::OnlyInternalOwners() const noexcept
    {
        // 引用数包含本对象的所有包 lease 和已保活父对象的真实边; 任何额外引用仍视为外部 owner.
        return m_Resource->GetRefCount() == m_Owners->packetHolds + m_Owners->dependencyHolds;
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

    // 在读取外部 footprint 前检查尺寸乘法, false 时不写结果.
    static bool CheckedMultiply(std::size_t a, std::size_t b, std::size_t &result) noexcept
    {
        if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b)
        {
            return false;
        }
        result = a * b;
        return true;
    }

    // count 已由非零 mip 尺寸保证, 计算最后有效 texel 的末端, 不把末尾 padding 纳入可读范围.
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
        // 源 footprint 包含行/切片间 padding, tightBytes 只包含 texel. 乘加先受检, 单 depth 不消费 depthPitch.
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

    // 新 chunk 同时满足本次写入, 列表最小容量和设备映射对齐.
    static bool GetChunkCapacity(std::size_t size, std::size_t minimumChunk, std::size_t alignment,
                                 std::size_t &capacity) noexcept
    {
        return CheckedAlign(std::max(size, minimumChunk), alignment, capacity);
    }

    // 预检和实际分配共用尾部的对齐及范围判断.
    static bool FitChunkTail(std::size_t used, std::size_t capacity, std::size_t size,
                             std::size_t alignment, std::size_t &offset) noexcept
    {
        return CheckedAlign(used, alignment, offset) && offset <= capacity && size <= capacity - offset;
    }

    UploadAdmission OpenGLDevice::CheckUploadCapacity(std::size_t chunkSize,
                                                       std::span<const std::size_t> textureWrites,
                                                       std::span<const std::size_t> bufferWrites) const noexcept
    {
        if (!RequireContext("CheckUploadCapacity") || m_ActiveRecording.load(std::memory_order_relaxed) != 0)
        {
            return UploadAdmission::InvalidState;
        }
        const auto alignment = std::size_t(m_Caps.minMapAlignment);
        std::size_t alignedChunk = 0;
        if (chunkSize == 0 || !GetChunkCapacity(0, chunkSize, alignment, alignedChunk) ||
            alignedChunk > UploadCapacityLimit)
        {
            return UploadAdmission::InvalidState;
        }

        // 按录制顺序模拟本包的首个可用尾部. 这里只记 CPU 范围, 实际 chunk 仍由录制取得.
        // 每笔非空写入最多增加一个 chunk. 写入次数的上限同时限制模拟区段数.
        struct ChunkRange
        {
            std::size_t used;
            std::size_t capacity;
        };
        std::array<ChunkRange, UploadItemLimit> chunks;
        std::size_t chunkCount = 0;
        std::size_t mappedCapacity = 0;
        std::size_t logicalBytes = 0;
        std::size_t writes = 0;
        const auto account = [&](std::size_t size, bool mapped)
        {
            if (size == 0)
            {
                return true;
            }
            if (writes == UploadItemLimit || size > UploadCapacityLimit - logicalBytes)
            {
                return false;
            }
            ++writes;
            logicalBytes += size;
            if (!mapped)
            {
                return true;
            }
            for (auto &chunk : std::span(chunks).first(chunkCount))
            {
                std::size_t offset = 0;
                if (FitChunkTail(chunk.used, chunk.capacity, size, alignment, offset))
                {
                    chunk.used = offset + size;
                    return true;
                }
            }

            std::size_t capacity = 0;
            if (!GetChunkCapacity(size, chunkSize, alignment, capacity) ||
                capacity > UploadCapacityLimit - mappedCapacity)
            {
                return false;
            }
            chunks[chunkCount++] = {size, capacity};
            mappedCapacity += capacity;
            return true;
        };
        // 小缓冲只占逻辑载荷和次数. 先核对空闲设备能否录制, 再扣除实际未退休容量.
        for (const auto size : textureWrites)
        {
            if (!account(size, true))
            {
                return UploadAdmission::ExceedsLimit;
            }
        }
        for (const auto size : bufferWrites)
        {
            if (!account(size, size > SmallBufferLimit))
            {
                return UploadAdmission::ExceedsLimit;
            }
        }
        return mappedCapacity <= UploadCapacityLimit - m_UploadCapacity ? UploadAdmission::Ready :
                                                                         UploadAdmission::Pending;
    }

    UploadAdmission CheckUploadCapacity(Device *device, std::size_t chunkSize,
                                         std::span<const std::size_t> textureWrites,
                                         std::span<const std::size_t> bufferWrites) noexcept
    {
        const auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native != nullptr ? native->CheckUploadCapacity(chunkSize, textureWrites, bufferWrites) :
                                   UploadAdmission::InvalidState;
    }

    bool OpenGLDevice::AllocateUpload(UploadWorkPacket &packet, std::size_t size, UploadSlice &slice)
    {
        // 只从当前包已有 chunk 的未使用尾部取空间. 新 chunk 同时受对齐/设备总预算限制, 不等待 GPU 腾空间.
        const auto alignment = std::size_t(m_Caps.minMapAlignment);
        for (auto &chunk : packet.chunks)
        {
            std::size_t offset = 0;
            if (chunk->mapping != nullptr && FitChunkTail(chunk->used, chunk->capacity, size, alignment, offset))
            {
                slice = {chunk->name, offset, chunk->mapping + offset};
                chunk->used = offset + size;
                return true;
            }
        }
        std::size_t capacity = 0;
        if (!GetChunkCapacity(size, packet.minimumChunk, alignment, capacity) ||
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
