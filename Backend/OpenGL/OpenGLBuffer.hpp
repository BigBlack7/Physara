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

        void *Map() override;  // dynamic=true -> 返回m_persistentPtr; static -> assert
        void Unmap() override; // Persistent Mapping下为空操作
        void UploadData(const void *data, std::uint32_t size, std::uint32_t offset = 0) override; // glNamedBufferSubData(static buffer的一次性上传)

        GLuint GetGLID() const { return m_id; }

    private:
        GLuint m_id{0};
        std::uint32_t m_size{0};
        BufferUsageFlags m_usage{0};
        bool m_dynamic{false};
        void *m_persistentPtr{nullptr}; // Persistent Mapping指针, dynamic=true时有效
    };
}