#include "CommandList.hpp"

/// @file
/// @brief 使用单一 Common tracker 处理有序传输/附件与只读图形用途, 缓存命中不替代依赖.

#include <new>

namespace Physara::RHI::OpenGL
{
    bool OpenGLCommandList::Retain(Resource *resource, InternalOwnership &owners)
    {
        // 叶资源在一个包中只取得一份引用; 图形复合对象由 RetainResource 先展开真实依赖边.
        // 后续准备可能已向 tracker 留下 extension 借用, 即使写入被拒绝, 此引用也随包统一退休.
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
            m_Packet->resources.emplace_back(*m_Device, resource, owners);
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

    // 只允许已实现命令的用途; keepInitialState 的合法恢复 metadata 不代表执行该能力.
    static bool TransferStateSupported(ResourceStates state, ResourceStates initial, bool keepInitial) noexcept
    {
        const auto reads = ResourceStates::ConstantBuffer | ResourceStates::VertexBuffer | ResourceStates::IndexBuffer |
                           ResourceStates::ShaderResource | ResourceStates::DepthRead;
        return state == ResourceStates::Common || state == ResourceStates::CopyDest ||
               state == ResourceStates::CopySource || state == ResourceStates::RenderTarget ||
               state == ResourceStates::DepthWrite ||
               state == (ResourceStates::CopySource | ResourceStates::CopyDest) ||
               (state != ResourceStates::Unknown && (unsigned(state) & ~unsigned(reads)) == 0) ||
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
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Packet->invalid = true;
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
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Packet->invalid = true;
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
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Packet->invalid = true;
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
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState))
        {
            m_Packet->invalid = true;
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
        return native != nullptr && Retain(native, native->m_InternalOwners)
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
        return native != nullptr && Retain(native, native->m_InternalOwners)
                   ? m_Packet->tracker.GetTextureSubresourceState(&native->m_State, slice, mip)
                   : ResourceStates::Unknown;
    }

    void OpenGLCommandList::CommitBarriers()
    {
        if (Recording("CommitBarriers") != nullptr)
        {
            static_cast<void>(ConsumeBarriers());
        }
    }

    bool OpenGLCommandList::TransferState(OpenGLBuffer &buffer)
    {
        return UseBuffer(buffer, ResourceStates::CopyDest, "WriteBuffer");
    }

    bool OpenGLCommandList::TransferState(OpenGLTexture &texture, TextureSubresourceSet subresources)
    {
        return UseTexture(texture, subresources, ResourceStates::CopyDest, "WriteTexture");
    }

    bool OpenGLCommandList::ConsumeBarriers()
    {
        if (m_Packet->invalid || m_Packet->tracker.HasErrors())
        {
            m_Packet->invalid = true;
            return false;
        }
        // 本 profile 的实际写仅有 coherent transfer/attachment, Shader 资源只读.
        // GL 有序命令已满足这些域的可见性, 不从恢复 metadata 臆造 Shader 写或发全域 memory barrier.
        m_Packet->tracker.ClearBarriers();
        return true;
    }

    bool OpenGLCommandList::UseBuffer(OpenGLBuffer &buffer, ResourceStates state, const char *operation)
    {
        // 自动模式请求后消费当前有序依赖; 显式模式只核对已请求且已 Commit 的状态, 不代调用方补齐.
        if (m_Packet->automaticBarriers)
        {
            m_Packet->tracker.RequireBufferState(&buffer.m_State, state);
            if (!ConsumeBarriers())
            {
                return false;
            }
        }
        const auto current = m_Packet->tracker.GetBufferState(&buffer.m_State);
        const bool ready = !m_Packet->tracker.HasErrors() && (unsigned(current) & unsigned(state)) == unsigned(state) &&
                           m_Packet->tracker.GetBufferBarriers().empty();
        if (!ready)
        {
            m_Packet->invalid = true;
            m_Device->Report(operation, "InvalidState: required buffer use and committed dependencies missing");
        }
        return ready;
    }

    bool OpenGLCommandList::UseTexture(OpenGLTexture &texture, TextureSubresourceSet range, ResourceStates state,
                                       const char *operation)
    {
        // view/传输范围在入口已解析, 此处逐 mip/layer 核对用途, 不以首个子资源代表整个视图.
        if (m_Packet->automaticBarriers)
        {
            m_Packet->tracker.RequireTextureState(&texture.m_State, range, state);
            if (!ConsumeBarriers())
            {
                return false;
            }
        }
        bool ready = !m_Packet->tracker.HasErrors() && m_Packet->tracker.GetTextureBarriers().empty();
        const auto resolved = range.Resolve(texture.GetDesc(), false);
        for (auto layer = resolved.baseArraySlice; layer < resolved.baseArraySlice + resolved.numArraySlices; ++layer)
        {
            for (auto mip = resolved.baseMipLevel; mip < resolved.baseMipLevel + resolved.numMipLevels; ++mip)
            {
                const auto current = m_Packet->tracker.GetTextureSubresourceState(&texture.m_State, layer, mip);
                ready = ready && (unsigned(current) & unsigned(state)) == unsigned(state);
            }
        }
        if (!ready)
        {
            m_Packet->invalid = true;
            m_Device->Report(operation, "InvalidState: required texture use and committed dependencies missing");
        }
        return ready;
    }
}
