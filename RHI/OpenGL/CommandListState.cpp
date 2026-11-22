#include "CommandList.hpp"

/// @file
/// @brief 在 Common tracker 中准备公共状态, 并把图授权与实际访问匹配到 Buffer 字节或纹理子资源.

#include <algorithm>
#include <new>
#include <ranges>

namespace Physara::RHI::OpenGL
{
    void OpenGLCommandList::RecordDepthWrite(OpenGLTexture &texture, TextureSubresourceSet subresources) noexcept
    {
        if (!texture.m_DepthWrites)
        {
            return;
        }
        // 一次 Draw 或整次清除涉及的 mip 共享同一写入序号. 联合采样能力限于单层 2D 存储.
        const auto epoch = ++m_Device->m_DepthWriteEpoch;
        for (auto mip = subresources.baseMipLevel; mip < subresources.baseMipLevel + subresources.numMipLevels; ++mip)
        {
            texture.m_DepthWrites[mip] = epoch;
        }
    }

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

    // 只要请求含间接参数位, 资源就须是带参数标志的 Buffer. 列表关闭另由 tracker 恢复初态.
    static bool TransferStateSupported(ResourceStates state, ResourceStates initial, bool keepInitial,
                                       bool indirectArgs, bool unordered) noexcept
    {
        if (!indirectArgs && (state & ResourceStates::IndirectArgument) != ResourceStates::Unknown)
        {
            return false;
        }
        if ((state & ResourceStates::UnorderedAccess) != ResourceStates::Unknown)
        {
            return unordered && state == ResourceStates::UnorderedAccess;
        }
        const auto reads = ResourceStates::ConstantBuffer | ResourceStates::VertexBuffer | ResourceStates::IndexBuffer |
                           ResourceStates::IndirectArgument | ResourceStates::ShaderResource | ResourceStates::DepthRead;
        return state == ResourceStates::Common || state == ResourceStates::CopyDest ||
               state == ResourceStates::CopySource || state == ResourceStates::RenderTarget ||
               state == ResourceStates::DepthWrite ||
               state == (ResourceStates::CopySource | ResourceStates::CopyDest) ||
               (state != ResourceStates::Unknown && (unsigned(state) & ~unsigned(reads)) == 0) ||
               (keepInitial && state == initial && ValidInitialState(initial, true));
    }

    UploadWorkPacket *OpenGLCommandList::StateRecording(const char *operation, bool graphRequest)
    {
        // 从同一个包取得 tracker. 只有接管入口能在 GraphManaged 中登记前态或修改状态.
        auto *packet = Recording(operation);
        if (packet && (packet->useMode == UseMode::GraphManaged) != graphRequest)
        {
            m_Device->Report(operation, "InvalidState: state preparation must match the recording mode");
            return nullptr;
        }
        return packet;
    }

    void OpenGLCommandList::SetEnableAutomaticBarriers(bool enable)
    {
        if (auto *packet = StateRecording("SetEnableAutomaticBarriers"))
        {
            packet->useMode = enable ? UseMode::Automatic : UseMode::Explicit;
        }
    }

    void OpenGLCommandList::BeginTrackingBufferState(Buffer *buffer, ResourceStates state)
    {
        SetTrackedBufferState(buffer, state, true, false);
    }

    void OpenGLCommandList::SetBufferState(Buffer *buffer, ResourceStates state)
    {
        SetTrackedBufferState(buffer, state, false, false);
    }

    void OpenGLCommandList::TrackGraphBuffer(Buffer &buffer, ResourceStates state)
    {
        SetTrackedBufferState(&buffer, state, true, true);
    }

    void OpenGLCommandList::RequestGraphBuffer(Buffer &buffer, ResourceStates state)
    {
        SetTrackedBufferState(&buffer, state, false, true);
    }

    void OpenGLCommandList::SetTrackedBufferState(Buffer *buffer, ResourceStates state, bool initial, bool graphRequest)
    {
        const auto *operation = initial ? "BeginTrackingBufferState" : "SetBufferState";
        auto *packet = StateRecording(operation, graphRequest);
        if (!packet)
        {
            return;
        }
        auto *native = BufferFor(buffer, operation);
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        if (!TransferStateSupported(state, native->m_Desc.initialState, native->m_Desc.keepInitialState,
                                    native->m_Desc.isDrawIndirectArgs, native->m_Desc.canHaveUAVs))
        {
            packet->invalid = true;
            m_Device->Report(operation, "Unsupported: buffer state or resource role");
            return;
        }
        if (initial)
        {
            packet->tracker.BeginTrackingBufferState(&native->m_State, state);
        }
        else
        {
            packet->tracker.RequireBufferState(&native->m_State, state);
        }
    }

    void OpenGLCommandList::BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                                      ResourceStates state)
    {
        SetTrackedTextureState(texture, subresources, state, true, false);
    }

    void OpenGLCommandList::SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        SetTrackedTextureState(texture, subresources, state, false, false);
    }

    void OpenGLCommandList::TrackGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state)
    {
        SetTrackedTextureState(&texture, range, state, true, true);
    }

    void OpenGLCommandList::RequestGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state)
    {
        SetTrackedTextureState(&texture, range, state, false, true);
    }

    void OpenGLCommandList::SetTrackedTextureState(Texture *texture, TextureSubresourceSet range, ResourceStates state,
                                                   bool initial, bool graphRequest)
    {
        const auto *operation = initial ? "BeginTrackingTextureState" : "SetTextureState";
        auto *packet = StateRecording(operation, graphRequest);
        if (!packet)
        {
            return;
        }
        auto *native = TextureFor(texture, operation);
        if (native == nullptr || !Retain(native, native->m_InternalOwners))
        {
            return;
        }
        const auto &desc = native->m_Desc;
        bool supported = false;
        if (state == ResourceStates::ResolveSource || state == ResourceStates::ResolveDest)
        {
            // 跟踪单个端点时检查其目标类型和样本能力. 两端所选 mip 的尺寸在实际 ResolveTexture 中配对.
            const auto caps = m_Device->GetTransferFormatSupport(desc.format, desc.dimension);
            supported = desc.isRenderTarget && (state == ResourceStates::ResolveSource
                ? (caps.resolveSourceSamples & (1u << desc.sampleCount)) != 0 : caps.resolveDestination);
        }
        else
        {
            supported = TransferStateSupported(state, desc.initialState, desc.keepInitialState, false, desc.isUAV);
        }
        if (!supported)
        {
            packet->invalid = true;
            m_Device->Report(operation, "Unsupported: texture state or resource role");
            return;
        }
        if (initial)
        {
            packet->tracker.BeginTrackingTextureState(&native->m_State, range, state);
        }
        else
        {
            packet->tracker.RequireTextureState(&native->m_State, range, state);
        }
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
        if (StateRecording("CommitBarriers"))
        {
            static_cast<void>(ConsumeBarriers());
        }
    }

    void OpenGLCommandList::CommitGraphUses(std::span<const GraphTextureUse> textures,
                                           std::span<const GraphBufferUse> buffers)
    {
        constexpr auto Operation = "CommitGraphUses";
        auto *packet = StateRecording(Operation, true);
        if (!packet || !ConsumeBarriers())
        {
            return;
        }
        // tracker 仍保存完整物理状态. 当前授权复制原声明的用途和范围, 同态请求也必须纳入本批.
        auto &uses = packet->graphUses;
        try
        {
            uses.textures.assign(textures.begin(), textures.end());
            uses.buffers.assign(buffers.begin(), buffers.end());
        }
        catch (const std::bad_alloc &)
        {
            // 两张表可能只完成一张. 整批撤销并锁存录制失败, 由原包保留已发前缀.
            uses.Reset();
            packet->invalid = true;
            m_Device->Report(Operation, "Could not store GraphManaged uses");
        }
    }

    void OpenGLCommandList::RevokeGraphUses() noexcept
    {
        // 接管对象仍持有原包, 即使本阶段已失效也可撤销 CPU 授权.
        m_Packet->graphUses.Reset();
    }

    bool OpenGLCommandList::ConsumeBarriers()
    {
        if (m_Packet->invalid || m_Packet->tracker.HasErrors())
        {
            m_Packet->invalid = true;
            return false;
        }
        // 此处只提交公共用途. 实际 native consumer 另按精确范围和指令域建立依赖.
        m_Packet->tracker.ClearBarriers();
        return true;
    }

    // 先匹配物理 Buffer, 公共状态和 storage 方向. 精确字节范围由具体访问另行核对.
    static bool MatchesBufferUse(const GraphBufferUse &use, const OpenGLBuffer &buffer,
                                 ResourceStates state, StorageAccess storage) noexcept
    {
        return use.buffer == &buffer && (use.state & state) == state && CoversStorageAccess(use.storage, storage);
    }

    // 图中两个范围均为非空半开区间. 按声明中剩余的字节数判断覆盖, 避免起点加长度溢出.
    static bool CoversBufferRange(BufferRange declared, BufferRange requested) noexcept
    {
        if (requested.byteOffset < declared.byteOffset)
        {
            return false;
        }
        const auto offset = requested.byteOffset - declared.byteOffset;
        return offset < declared.byteSize && requested.byteSize <= declared.byteSize - offset;
    }

    bool OpenGLCommandList::CheckGraphBufferUse(const OpenGLBuffer &buffer, ResourceStates state,
                                               const char *operation, std::optional<BufferRange> range,
                                               bool structuredBinding, StorageAccess storage)
    {
        if (m_Packet->useMode != UseMode::GraphManaged)
        {
            return true;
        }
        const auto &uses = m_Packet->graphUses.buffers;
        const bool allowed = std::ranges::any_of(uses, [&](const GraphBufferUse &use)
        {
            // 无范围仅识别角色. 表绑定须精确匹配已声明的 indexedBinding, 实际读取仍受声明行范围限制.
            return MatchesBufferUse(use, buffer, state, storage) &&
                   (!range || (structuredBinding && use.indexedBinding == range) ||
                    CoversBufferRange(use.range, *range));
        });
        if (!allowed)
        {
            m_Packet->invalid = true;
            m_Device->Report(operation, "InvalidState: required buffer use or byte range was not prepared",
                             buffer.GetDesc().debugName.c_str());
        }
        return allowed;
    }

    bool OpenGLCommandList::UseBuffer(OpenGLBuffer &buffer, ResourceStates state, const char *operation,
                                      std::optional<BufferRange> range, bool structuredBinding, StorageAccess storage,
                                      GLbitfield domain, bool replaces)
    {
        // 带用途的精确权限独立于整对象 tracker 状态, 防止不同字节区间互相授权.
        if (!CheckGraphBufferUse(buffer, state, operation, range, structuredBinding, storage))
        {
            return false;
        }
        if (m_Packet->useMode == UseMode::Automatic)
        {
            m_Packet->tracker.RequireBufferState(&buffer.m_State, state);
            if (!ConsumeBarriers())
            {
                return false;
            }
        }
        const auto current = m_Packet->tracker.GetBufferState(&buffer.m_State);
        const bool ready = !m_Packet->tracker.HasErrors() &&
                           (unsigned(current) & unsigned(state)) == unsigned(state) &&
                           m_Packet->tracker.GetBufferBarriers().empty();
        if (!ready)
        {
            m_Packet->invalid = true;
            m_Device->Report(operation, "InvalidState: required buffer use and committed dependencies missing");
        }
        if (!ready || domain == 0 || buffer.m_ShaderAccess.empty())
        {
            return ready;
        }
        const auto record = [&](BufferRange bytes)
        {
            return AddNativeAccess({.buffer = &buffer, .bytes = bytes, .domain = domain,
                                    .shaderWrite = CoversStorageAccess(storage, StorageAccess::Write),
                                    .replaces = replaces}, operation);
        };
        if (structuredBinding && range && m_Packet->useMode == UseMode::GraphManaged)
        {
            const auto &uses = m_Packet->graphUses.buffers;
            // 有声明覆盖整次实际绑定时登记该绑定. 仅凭 indexedBinding 取得许可时须回到匹配的各个行范围.
            const bool wholeRange = std::ranges::any_of(uses, [&](const GraphBufferUse &use)
            {
                return MatchesBufferUse(use, buffer, state, storage) && CoversBufferRange(use.range, *range);
            });
            if (!wholeRange)
            {
                // 前面的授权已验证绑定身份. 收集同一 SRV 的每个声明行, 不把整个绑定登记成 Shader 读取.
                for (const auto &use : uses)
                {
                    if (MatchesBufferUse(use, buffer, state, storage) &&
                        use.indexedBinding == range && !record(use.range))
                    {
                        return false;
                    }
                }
                return true;
            }
        }
        return record(range.value_or(BufferRange{0, buffer.GetDesc().byteSize}));
    }

    bool OpenGLCommandList::UseTexture(OpenGLTexture &texture, TextureSubresourceSet range, TextureAspect aspects,
                                       ResourceStates state, const char *operation, ResourceStates access, StorageAccess storage,
                                       GLbitfield domain, bool replaces)
    {
        // 在整个 view 或传输范围逐 mip/layer 核对 tracker 状态和当前批次覆盖.
        if (m_Packet->useMode == UseMode::Automatic)
        {
            m_Packet->tracker.RequireTextureState(&texture.m_State, range, state);
            if (!ConsumeBarriers())
            {
                return false;
            }
        }
        bool ready = !m_Packet->tracker.HasErrors() && m_Packet->tracker.GetTextureBarriers().empty();
        const auto &uses = m_Packet->graphUses;
        const bool graphManaged = m_Packet->useMode == UseMode::GraphManaged;
        // 联合用途检查合并后的跟踪状态, 采样与附件分别匹配各自的访问权限.
        const auto required = access == ResourceStates::Unknown ? state : access;
        const auto resolved = range.Resolve(texture.GetDesc(), false);
        for (auto layer = resolved.baseArraySlice; layer < resolved.baseArraySlice + resolved.numArraySlices; ++layer)
        {
            for (auto mip = resolved.baseMipLevel; mip < resolved.baseMipLevel + resolved.numMipLevels; ++mip)
            {
                const auto current = m_Packet->tracker.GetTextureSubresourceState(&texture.m_State, layer, mip);
                ready = ready && (unsigned(current) & unsigned(state)) == unsigned(state);
                if (ready && graphManaged)
                {
                    // 在当前物理单元累加同用途的分量位. All 的零位值在这里作为空位集使用.
                    auto covered = TextureAspect::All;
                    for (const auto &use : uses.textures)
                    {
                        const auto &requested = use.range;
                        if (use.texture == &texture && (use.state & required) == required &&
                            CoversStorageAccess(use.storage, storage) &&
                            layer >= requested.baseArraySlice &&
                            layer - requested.baseArraySlice < requested.numArraySlices &&
                            mip >= requested.baseMipLevel && mip - requested.baseMipLevel < requested.numMipLevels)
                        {
                            covered = covered | use.aspect;
                        }
                    }
                    ready = (covered & aspects) == aspects;
                }
            }
        }
        if (!ready)
        {
            m_Packet->invalid = true;
            m_Device->Report(operation, "InvalidState: required texture use and committed dependencies missing");
        }
        return ready && (domain == 0 || texture.m_ShaderAccess.empty() ||
            AddNativeAccess({.texture = &texture, .subresources = resolved, .domain = domain,
                             .shaderWrite = CoversStorageAccess(storage, StorageAccess::Write),
                             .replaces = replaces}, operation));
    }
}
