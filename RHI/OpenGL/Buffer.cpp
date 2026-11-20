#include "Backend.hpp"

/// @file
/// @brief 创建设备拥有的普通或 volatile Buffer. 公开描述保存逻辑容量, 多版本常量另分配对齐的映射存储.

#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <utility>
#include "Upload.hpp"

namespace Physara::RHI::OpenGL
{
    OpenGLBuffer::OpenGLBuffer(OpenGLDevice *device, const BufferDesc &desc)
        : m_Device(device), m_Desc(desc), m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Buffer))
    {
    }

    OpenGLBuffer::~OpenGLBuffer()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    // 先校验尺寸, usage 与当前 profile, 再分配 immutable storage, 全部成功后才发布 owning handle.
    BufferHandle OpenGLDevice::CreateBuffer(const BufferDesc &desc)
    {
        if (!RequireContext("CreateBuffer"))
        {
            return nullptr;
        }
        if (desc.byteSize == 0 || desc.byteSize > std::uint64_t(std::numeric_limits<GLsizeiptr>::max()) ||
            (desc.structStride != 0 && (desc.structStride > desc.byteSize || desc.byteSize % desc.structStride != 0)) ||
            (desc.canHaveRawViews && desc.byteSize % 4 != 0) ||
            !ValidInitialState(desc.initialState, desc.keepInitialState))
        {
            Report("CreateBuffer", "InvalidArgument: size, layout or initial state", desc.debugName.c_str());
            return nullptr;
        }
        if (desc.cpuAccess != CpuAccessMode::None || desc.isVirtual || desc.canHaveTypedViews ||
            desc.format != Format::UNKNOWN || desc.sharedResourceFlags != SharedResourceFlags::None ||
            desc.isAccelStructBuildInput || desc.isAccelStructStorage || desc.isShaderBindingTable)
        {
            Report("CreateBuffer", "Unsupported: buffer profile", desc.debugName.c_str());
            return nullptr;
        }
        std::size_t storageBytes = static_cast<std::size_t>(desc.byteSize);
        std::size_t stride = 0;
        if (desc.isVolatile)
        {
            if (!desc.isConstantBuffer || desc.maxVersions == 0 || desc.structStride != 0 || desc.isVertexBuffer ||
                desc.isIndexBuffer || desc.isDrawIndirectArgs || desc.canHaveUAVs || desc.canHaveRawViews ||
                desc.byteSize > static_cast<std::uint64_t>(m_Caps.shader.uniformBlockBytes))
            {
                Report("CreateBuffer", "InvalidArgument: volatile constant-buffer description", desc.debugName.c_str());
                return nullptr;
            }
            // 每槽起点同时满足映射和 UBO range 的对齐. 原生分配和设备预算均使用所有槽的总字节数.
            const auto alignment = std::lcm(std::size_t(m_Caps.minMapAlignment),
                                            std::size_t(m_Caps.shader.uniformAlignment));
            if (!CheckedAlign(storageBytes, alignment, stride))
            {
                char reason[128];
                std::snprintf(reason, sizeof(reason),
                              "Aligned stride unrepresentable: bytes=%zu alignment=%zu versions=%u",
                              storageBytes, alignment, desc.maxVersions);
                Report("CreateBuffer.VolatileBacking", reason, desc.debugName.c_str());
                return nullptr;
            }
            if (stride > std::numeric_limits<std::size_t>::max() / desc.maxVersions)
            {
                char reason[128];
                std::snprintf(reason, sizeof(reason), "Total backing unrepresentable: stride=%zu versions=%u",
                              stride, desc.maxVersions);
                Report("CreateBuffer.VolatileBacking", reason, desc.debugName.c_str());
                return nullptr;
            }
            storageBytes = stride * desc.maxVersions;
            if (storageBytes > VolatileCapacityLimit - m_VolatileCapacity)
            {
                ReportBudget("CreateBuffer.VolatileBacking", storageBytes, m_VolatileCapacity, VolatileCapacityLimit);
                return nullptr;
            }
        }
        try
        {
            // 字符串副本/删除节点的分配必须先于原生 name. wrapper 发布前的任何返回都会清理本次取得的对象.
            auto buffer = std::make_unique<OpenGLBuffer>(this, desc);
            if (desc.isVolatile)
            {
                buffer->m_Versions.resize(desc.maxVersions);
                buffer->m_VersionStride = stride;
            }
            if (!BeginNative("CreateBuffer", desc.debugName.c_str()))
            {
                return nullptr;
            }
            glCreateBuffers(1, &buffer->m_Deletion->name);
            if (!CheckNative("CreateBuffer.Name", buffer->m_Deletion->name, desc.debugName.c_str()))
            {
                return nullptr;
            }
            if (buffer->m_Deletion->name == 0)
            {
                m_Phase = Phase::Faulted;
                Report("CreateBuffer", "NativeFailure: zero object name", desc.debugName.c_str());
                return nullptr;
            }
            constexpr GLbitfield Mapping = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            glNamedBufferStorage(buffer->m_Deletion->name, GLsizeiptr(storageBytes), nullptr,
                                 desc.isVolatile ? Mapping : GL_DYNAMIC_STORAGE_BIT);
            if (!CheckNative("CreateBuffer.Storage", buffer->m_Deletion->name, desc.debugName.c_str()))
            {
                return nullptr;
            }
            if (desc.isVolatile)
            {
                buffer->m_Mapping = static_cast<std::byte *>(
                    glMapNamedBufferRange(buffer->Name(), 0, GLsizeiptr(storageBytes), Mapping));
                const bool ok = CheckNative("CreateBuffer.VolatileMap", buffer->Name(), desc.debugName.c_str());
                if (!ok || !buffer->m_Mapping)
                {
                    if (ok)
                    {
                        Report("CreateBuffer", "NativeFailure: persistent constant-buffer mapping unavailable",
                               desc.debugName.c_str());
                    }
                    return nullptr;
                }
                buffer->m_Deletion->volatileBytes = storageBytes;
                m_VolatileCapacity += storageBytes;
            }
            buffer->Publish();
            return BufferHandle::Create(buffer.release());
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateBuffer", "CPU allocation failed", desc.debugName.c_str());
            return nullptr;
        }
    }
}
