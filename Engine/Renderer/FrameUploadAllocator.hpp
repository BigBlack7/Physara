#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    struct FrameStatistics;

    struct FrameUploadAllocation
    {
        RHI::RHIBuffer *buffer{nullptr};
        std::uint32_t offset{0};
        std::uint32_t size{0};

        [[nodiscard]] bool IsValid() const { return buffer != nullptr && size > 0u; }
    };

    class FrameUploadAllocator final
    {
    public:
        void Reset();
        void Release();

        [[nodiscard]] FrameUploadAllocation Upload(
            RHI::RHIDevice &device,
            const void *data,
            std::uint32_t size,
            FrameStatistics *stats = nullptr);
        void Flush(FrameStatistics *stats = nullptr);

        template <typename T>
        [[nodiscard]] FrameUploadAllocation Upload(RHI::RHIDevice &device, const T &value, FrameStatistics *stats = nullptr)
        {
            return Upload(device, &value, static_cast<std::uint32_t>(sizeof(T)), stats);
        }

    private:
        void AllocateActiveBuffer(RHI::RHIDevice &device, std::uint32_t requiredBytes, FrameStatistics *stats);
        void MarkDirtyRange(std::uint32_t offset, std::uint32_t size);
        [[nodiscard]] static std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment);

    private:
        static constexpr std::uint32_t kDefaultCapacity = 1024u * 1024u;
        static constexpr std::uint32_t kAlignment = 256u;

        std::unique_ptr<RHI::RHIBuffer> m_Buffer{};
        std::vector<std::unique_ptr<RHI::RHIBuffer>> m_RetiredFrameBuffers{};
        std::vector<std::uint8_t> m_Staging{};
        std::uint32_t m_Capacity{0};
        std::uint32_t m_Offset{0};
        std::uint32_t m_DirtyStart{0};
        std::uint32_t m_DirtyEnd{0};
        bool m_HasDirtyRange{false};
    };
}
