#include "OpenGLBuffer.hpp"

#include <cassert>

namespace Physara::RHI
{
    OpenGLBuffer::OpenGLBuffer(const RHIBufferDesc &desc) : m_Size(desc.size), m_Usage(desc.usage), m_Dynamic(desc.dynamic)
    {
        assert(m_Size > 0);

        // DSA创建buffer object; 后续所有写入都通过named buffer API, 不依赖GL_ARRAY_BUFFER等绑定点
        glCreateBuffers(1, &m_ID);

        m_SegmentStride = m_Size;
        if (m_Dynamic)
        {
            GLint uniformAlignment = 1;
            GLint storageAlignment = 1;
            glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniformAlignment);
            glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &storageAlignment);
            const std::uint32_t alignment = static_cast<std::uint32_t>(uniformAlignment > storageAlignment ? uniformAlignment : storageAlignment);
            m_SegmentStride = ((m_Size + alignment - 1u) / alignment) * alignment;
        }
        m_StorageSize = m_Dynamic ? m_SegmentStride * kDynamicRingSegments : m_Size;

        // 动态buffer使用多段ring storage错开常规帧写入，不在上传热路径插入显式fence查询。
        GLbitfield storageFlags = GL_DYNAMIC_STORAGE_BIT;
        if (m_Dynamic)
        {
            storageFlags |= GL_MAP_WRITE_BIT;
        }

        // 创建并初始化缓冲区对象的不可变数据存储, 分配缓冲区内存空间, 设置访问权限和优化策略
        glNamedBufferStorage(
            m_ID,
            static_cast<GLsizeiptr>(m_StorageSize),
            m_Dynamic ? nullptr : desc.initialData,
            storageFlags);
        if (m_Dynamic && desc.initialData != nullptr)
        {
            glNamedBufferSubData(m_ID, 0, static_cast<GLsizeiptr>(m_Size), desc.initialData);
            m_HasActiveSegment = true;
        }
    }

    OpenGLBuffer::~OpenGLBuffer()
    {
        if (m_ID != 0)
        {
            if (m_MappedPtr != nullptr)
            {
                glUnmapNamedBuffer(m_ID);
                m_MappedPtr = nullptr;
            }
            glDeleteBuffers(1, &m_ID);
            m_ID = 0;
        }
    }

    std::uint32_t OpenGLBuffer::GetSize() const
    {
        return m_Size;
    }

    BufferUsageFlags OpenGLBuffer::GetUsage() const
    {
        return m_Usage;
    }

    void *OpenGLBuffer::Map()
    {
        // 当前只允许dynamic buffer map, 因为静态buffer没有创建MAP_*权限
        assert(m_Dynamic && "Map() only allowed for dynamic buffers.");
        assert(m_MappedPtr == nullptr && "Buffer is already mapped.");
        BeginDynamicWrite();
        m_MappedPtr = glMapNamedBufferRange(
            m_ID,
            static_cast<GLintptr>(GetBindOffset()),
            static_cast<GLsizeiptr>(m_Size),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        assert(m_MappedPtr != nullptr);
        return m_MappedPtr;
    }

    void OpenGLBuffer::Unmap()
    {
        if (m_MappedPtr == nullptr)
        {
            return;
        }

        glUnmapNamedBuffer(m_ID);
        m_MappedPtr = nullptr;
    }

    void OpenGLBuffer::UploadData(const void *data, std::uint32_t size, std::uint32_t offset)
    {
        assert(data != nullptr);
        assert((offset + size) <= m_Size);
        assert(m_MappedPtr == nullptr && "UploadData() called while buffer is mapped.");

        if (m_Dynamic && offset == 0u)
        {
            BeginDynamicWrite();
        }

        const std::uint32_t writeOffset = GetBindOffset() + offset;
        glNamedBufferSubData(m_ID, static_cast<GLintptr>(writeOffset), static_cast<GLsizeiptr>(size), data);
    }

    std::uint32_t OpenGLBuffer::GetBindOffset() const
    {
        return m_Dynamic ? m_CurrentSegment * m_SegmentStride : 0u;
    }

    void OpenGLBuffer::BeginDynamicWrite()
    {
        assert(m_Dynamic);
        if (m_HasActiveSegment)
        {
            m_CurrentSegment = (m_CurrentSegment + 1u) % kDynamicRingSegments;
        }

        m_HasActiveSegment = true;
    }
}