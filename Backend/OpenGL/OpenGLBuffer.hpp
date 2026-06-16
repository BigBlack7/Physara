#pragma once

#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class OpenGLBuffer final : public RHIBuffer
    {
    public:
        explicit OpenGLBuffer(const RHIBufferDesc &desc);
        ~OpenGLBuffer() override;

        std::uint32_t GetSize() const override;
        BufferUsageFlags GetUsage() const override;

        void *Map() override;
        void Unmap() override;
        void UploadData(const void *data, std::uint32_t size, std::uint32_t offset = 0) override; // glNamedBufferSubData(static buffer的一次性上传)

        GLuint GetGLID() const { return m_ID; }
        [[nodiscard]] std::uint32_t GetBindOffset() const;
        [[nodiscard]] std::uint32_t GetBindSize() const { return m_Size; }
        [[nodiscard]] bool IsDynamic() const { return m_Dynamic; }

    private:
        void BeginDynamicWrite();

    private:
        static constexpr std::uint32_t kDynamicRingSegments = 3u;

        GLuint m_ID{0};
        std::uint32_t m_Size{0};
        std::uint32_t m_SegmentStride{0};
        std::uint32_t m_StorageSize{0};
        BufferUsageFlags m_Usage{0};
        bool m_Dynamic{false};
        void *m_MappedPtr{nullptr};
        std::uint32_t m_CurrentSegment{0};
        bool m_HasActiveSegment{false};
    };
}