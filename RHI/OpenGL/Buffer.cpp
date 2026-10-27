#include "Backend.hpp"

#include <limits>
#include <memory>
#include <new>
#include <utility>

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
        if (desc.cpuAccess != CpuAccessMode::None || desc.isVolatile || desc.isVirtual || desc.canHaveTypedViews ||
            desc.format != Format::UNKNOWN || desc.sharedResourceFlags != SharedResourceFlags::None ||
            desc.isAccelStructBuildInput || desc.isAccelStructStorage || desc.isShaderBindingTable)
        {
            Report("CreateBuffer", "Unsupported: buffer profile", desc.debugName.c_str());
            return nullptr;
        }
        try
        {
            auto buffer = std::make_unique<OpenGLBuffer>(this, desc);
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
            glNamedBufferStorage(buffer->m_Deletion->name, GLsizeiptr(desc.byteSize), nullptr, GL_DYNAMIC_STORAGE_BIT);
            if (!CheckNative("CreateBuffer.Storage", buffer->m_Deletion->name, desc.debugName.c_str()))
            {
                return nullptr;
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
