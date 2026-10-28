#include "CommandList.hpp"

#include <new>

namespace Physara::RHI::OpenGL
{
    bool OpenGLCommandList::Retain(Resource *resource, unsigned long &holds)
    {
        for (const auto &lease : m_Packet->resources)
        {
            if (lease.Get() == resource)
            {
                return true;
            }
        }
        if (m_Packet->resources.size() == UploadItemLimit)
        {
            m_Device->ReportBudget("RetainUploadResource", 1, UploadItemLimit, UploadItemLimit);
            return false;
        }
        try
        {
            m_Packet->resources.emplace_back(*m_Device, resource, holds);
            return true;
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report("RetainUploadResource", "CPU allocation failed before native write");
            return false;
        }
    }

    OpenGLBuffer *OpenGLCommandList::BufferFor(Buffer *buffer, const char *operation)
    {
        auto *native = dynamic_cast<OpenGLBuffer *>(buffer);
        if (native == nullptr || native->m_Device.Get() != m_Device.Get())
        {
            m_Device->Report(operation, "InvalidArgument: buffer implementation or owning device");
            return nullptr;
        }
        return native;
    }

    OpenGLTexture *OpenGLCommandList::TextureFor(Texture *texture, const char *operation)
    {
        auto *native = dynamic_cast<OpenGLTexture *>(texture);
        if (native == nullptr || native->Owner() != m_Device.Get())
        {
            m_Device->Report(operation, "InvalidArgument: texture implementation or owning device");
            return nullptr;
        }
        return native;
    }

    static bool TransferStateSupported(ResourceStates state, ResourceStates initial, bool keepInitial) noexcept
    {
        return state == ResourceStates::Common || state == ResourceStates::CopyDest ||
               (keepInitial && state == initial && ValidInitialState(initial, true));
    }

    void OpenGLCommandList::SetEnableAutomaticBarriers(bool enable)
    {
        if (auto *packet = Recording("SetEnableAutomaticBarriers"))
        {
            packet->automaticBarriers = enable;
        }
    }

    void OpenGLCommandList::BeginTrackingBufferState(Buffer *buffer, ResourceStates state)
    {
        if (Recording("BeginTrackingBufferState") == nullptr)
        {
            return;
        }
        auto *native = BufferFor(buffer, "BeginTrackingBufferState");
        if (native == nullptr || !Retain(native, native->m_InternalUploadHolds))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Device->Report("BeginTrackingBufferState", "Unsupported: transfer state");
            return;
        }
        m_Packet->tracker.BeginTrackingBufferState(&native->m_State, state);
    }

    void OpenGLCommandList::SetBufferState(Buffer *buffer, ResourceStates state)
    {
        if (Recording("SetBufferState") == nullptr)
        {
            return;
        }
        auto *native = BufferFor(buffer, "SetBufferState");
        if (native == nullptr || !Retain(native, native->m_InternalUploadHolds))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Device->Report("SetBufferState", "Unsupported: transfer state");
            return;
        }
        m_Packet->tracker.RequireBufferState(&native->m_State, state);
    }

    void OpenGLCommandList::BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                                      ResourceStates state)
    {
        if (Recording("BeginTrackingTextureState") == nullptr)
        {
            return;
        }
        auto *native = TextureFor(texture, "BeginTrackingTextureState");
        if (native == nullptr || !Retain(native, native->m_InternalUploadHolds))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Device->Report("BeginTrackingTextureState", "Unsupported: transfer state");
            return;
        }
        m_Packet->tracker.BeginTrackingTextureState(&native->m_State, subresources, state);
    }

    void OpenGLCommandList::SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        if (Recording("SetTextureState") == nullptr)
        {
            return;
        }
        auto *native = TextureFor(texture, "SetTextureState");
        if (native == nullptr || !Retain(native, native->m_InternalUploadHolds))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Device->Report("SetTextureState", "Unsupported: transfer state");
            return;
        }
        m_Packet->tracker.RequireTextureState(&native->m_State, subresources, state);
    }

    ResourceStates OpenGLCommandList::GetBufferState(Buffer *buffer)
    {
        if (Recording("GetBufferState") == nullptr)
        {
            return ResourceStates::Unknown;
        }
        auto *native = BufferFor(buffer, "GetBufferState");
        return native != nullptr && Retain(native, native->m_InternalUploadHolds)
                   ? m_Packet->tracker.GetBufferState(&native->m_State)
                   : ResourceStates::Unknown;
    }

    ResourceStates OpenGLCommandList::GetTextureSubresourceState(Texture *texture, ArraySlice slice, MipLevel mip)
    {
        if (Recording("GetTextureSubresourceState") == nullptr)
        {
            return ResourceStates::Unknown;
        }
        auto *native = TextureFor(texture, "GetTextureSubresourceState");
        return native != nullptr && Retain(native, native->m_InternalUploadHolds)
                   ? m_Packet->tracker.GetTextureSubresourceState(&native->m_State, slice, mip)
                   : ResourceStates::Unknown;
    }

    void OpenGLCommandList::CommitBarriers()
    {
        if (Recording("CommitBarriers") != nullptr)
        {
            // 当前唯一生产者为有序 transfer, 没有 shader 写入可见性需求. 消费 Common 的抽象依赖.
            m_Packet->tracker.ClearBarriers();
        }
    }

    bool OpenGLCommandList::TransferState(OpenGLBuffer &buffer)
    {
        if (m_Packet->automaticBarriers)
        {
            m_Packet->tracker.RequireBufferState(&buffer.m_State, ResourceStates::CopyDest);
            m_Packet->tracker.ClearBarriers();
        }
        const bool ready = !m_Packet->tracker.HasErrors() &&
                           m_Packet->tracker.GetBufferState(&buffer.m_State) == ResourceStates::CopyDest &&
                           m_Packet->tracker.GetBufferBarriers().empty();
        if (!ready)
        {
            m_Device->Report("WriteBuffer", "InvalidState: CopyDest and consumed dependencies required");
        }
        return ready;
    }

    bool OpenGLCommandList::TransferState(OpenGLTexture &texture, TextureSubresourceSet subresources)
    {
        if (m_Packet->automaticBarriers)
        {
            m_Packet->tracker.RequireTextureState(&texture.m_State, subresources, ResourceStates::CopyDest);
            m_Packet->tracker.ClearBarriers();
        }
        const bool ready =
            !m_Packet->tracker.HasErrors() &&
            m_Packet->tracker.GetTextureSubresourceState(&texture.m_State, subresources.baseArraySlice,
                                                         subresources.baseMipLevel) == ResourceStates::CopyDest &&
            m_Packet->tracker.GetTextureBarriers().empty();
        if (!ready)
        {
            m_Device->Report("WriteTexture", "InvalidState: CopyDest and consumed dependencies required");
        }
        return ready;
    }
}
