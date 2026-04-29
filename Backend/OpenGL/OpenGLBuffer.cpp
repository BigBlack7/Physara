#include "OpenGLBuffer.hpp"

#include <cassert>

namespace Physara::RHI
{
    OpenGLBuffer::OpenGLBuffer(const RHIBufferDesc &desc) : m_Size(desc.size), m_Usage(desc.usage), m_Dynamic(desc.dynamic)
    {
        assert(m_Size > 0);

        glCreateBuffers(1, &m_ID);

        GLbitfield storageFlags = 0;
        if (m_Dynamic)
        {
            // 允许对映射区域进行写操作
            // 创建持久化映射, 映射在整个缓冲区生命周期内保持有效, 避免反复映射/取消映射的开销
            // 确保写入操作对GPU立即可见, 提供更简单的同步机制, 但可能影响性能
            storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        }

        // 创建并初始化缓冲区对象的不可变数据存储, 分配缓冲区内存空间, 设置访问权限和优化策略
        glNamedBufferStorage(m_ID, static_cast<GLsizeiptr>(m_Size), desc.initialData, storageFlags);

        if (m_Dynamic)
        {
            // 映射缓冲区对象的一个范围到客户端内存, 返回指向缓冲区内存的指针, 允许直接读写操作
            m_PersistentPtr = glMapNamedBufferRange(
                m_ID,
                0,
                static_cast<GLsizeiptr>(m_Size),
                storageFlags);

            assert(m_PersistentPtr != nullptr);
        }
    }

    OpenGLBuffer::~OpenGLBuffer()
    {
        if (m_ID != 0)
        {
            if (m_PersistentPtr != nullptr)
            {
                // 解除之前通过glMapNamedBufferRange或glMapNamedBuffer创建的缓冲区映射
                glUnmapNamedBuffer(m_ID);
                m_PersistentPtr = nullptr;
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
        assert(m_Dynamic && "Map() only allowed for dynamic buffers.");
        return m_PersistentPtr;
    }

    void OpenGLBuffer::Unmap()
    {
        // Persistent mapping: no-op by design.
        /* Note
            1. 普通glMapBuffer的工作方式是: Map → CPU 拿到指针写数据 → Unmap → GPU 才能使用这块内存,
            Unmap 是必须的, 因为 Map 期间 GPU 不能访问这块内存, Unmap 是"归还所有权给 GPU"的信号.

            2. Persistent Mapping(GL_MAP_PERSISTENT_BIT)打破了这个限制: 一次 Map → CPU 和 GPU 同时持有指针 → 永远不需要 Unmap,
            加上 GL_MAP_COHERENT_BIT 后, CPU 写入的数据对 GPU 自动可见, 不需要任何额外同步调用.
        */
    }

    void OpenGLBuffer::UploadData(const void *data, std::uint32_t size, std::uint32_t offset)
    {
        assert(data != nullptr);
        assert((offset + size) <= m_Size);
        // 更新命名缓冲区对象的一个数据子区域
        glNamedBufferSubData(m_ID, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    }
}