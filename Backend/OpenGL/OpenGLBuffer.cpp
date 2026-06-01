#include "OpenGLBuffer.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>

namespace Physara::RHI
{
    OpenGLBuffer::OpenGLBuffer(const RHIBufferDesc &desc) : m_Size(desc.size), m_Usage(desc.usage), m_Dynamic(desc.dynamic)
    {
        assert(m_Size > 0);

        // DSA创建buffer object; 后续所有写入都通过named buffer API, 不依赖GL_ARRAY_BUFFER等绑定点
        glCreateBuffers(1, &m_ID);

        // glNamedBufferStorage创建immutable storage. GL_DYNAMIC_STORAGE_BIT表示允许后续
        // glNamedBufferSubData。动态buffer暂时走DSA上传，避免无ring/fence的persistent映射造成CPU/GPU串行。
        GLbitfield storageFlags = GL_DYNAMIC_STORAGE_BIT;
        if (m_Dynamic)
        {
            storageFlags |= GL_MAP_WRITE_BIT;
        }

        // 创建并初始化缓冲区对象的不可变数据存储, 分配缓冲区内存空间, 设置访问权限和优化策略
        glNamedBufferStorage(m_ID, static_cast<GLsizeiptr>(m_Size), desc.initialData, storageFlags);
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
        m_MappedPtr = glMapNamedBufferRange(
            m_ID,
            0,
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

        // 静态/动态buffer都走DSA sub data更新。真正的高性能路径后续会升级为ring buffer + fence。
        glNamedBufferSubData(m_ID, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    }
}