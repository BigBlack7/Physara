#include "CommandList.hpp"

/// @file
/// @brief 在原存储的字节或 mip/layer 上记录实际访问. 在命令发出前合并所需的 GL 屏障位.

#include <algorithm>
#include <bit>
#include <limits>
#include <new>

namespace Physara::RHI::OpenGL
{
    // 按此前各段的右端点查找切口, 两个新区间继承相同历史. 触及区间上限时在原生命令前返回失败.
    static bool SplitSpan(std::vector<BufferAccessSpan> &history, std::uint64_t point)
    {
        const auto span = std::ranges::upper_bound(history, point, {}, &BufferAccessSpan::end);
        if (span == history.end() || span->first == point)
        {
            return true;
        }
        if (history.size() == UploadItemLimit)
        {
            return false;
        }
        const auto right = BufferAccessSpan{point, span->end, span->stamp};
        const auto index = std::size_t(span - history.begin());
        history.insert(span + 1, right);
        history[index].end = point;
        return true;
    }

    // 相邻区间的 shaderWrite 和 lastAccess 都相同时可以原地合并, 保留半开区间对整个 Buffer 的覆盖.
    static void MergeSpans(std::vector<BufferAccessSpan> &history) noexcept
    {
        std::size_t count = 0;
        for (const auto span : history)
        {
            if (count != 0 && history[count - 1].stamp == span.stamp)
            {
                history[count - 1].end = span.end;
            }
            else
            {
                history[count++] = span;
            }
        }
        history.erase(history.begin() + std::ptrdiff_t(count), history.end());
    }

    // 区间接近预算时, 先消除已满足的顺序约束, 再把具有相同待同步消费域的相邻段合并.
    static void CompactBufferSpans(std::vector<BufferAccessSpan> &history, std::uint64_t completed,
                                   const std::array<std::uint64_t, 32> &visible) noexcept
    {
        constexpr GLbitfield domains = GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
            GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT;
        // 后续 SSBO 写只需等到前序访问完成或被 storage 屏障排序. 这两种进度都可以清除 lastAccess.
        const auto ordered = (std::max)(completed, visible[std::countr_zero(GLbitfield{GL_SHADER_STORAGE_BARRIER_BIT})]);
        for (auto &span : history)
        {
            if (span.stamp.lastAccess <= ordered)
            {
                span.stamp.lastAccess = 0;
            }
            // 尚未可见的每个域都要求写序号大于其进度. 取这些进度的最大值加一, 恰好保留原来的域集合.
            // 下次为某域发出屏障时, 该域的进度会越过紧缩前的写入. GPU 完成本身只影响上面的排序判断.
            const auto written = span.stamp.shaderWrite;
            span.stamp.shaderWrite = 0;
            for (auto pending = domains; pending != 0; pending &= pending - 1)
            {
                const auto epoch = visible[std::countr_zero(pending)];
                if (epoch < written)
                {
                    span.stamp.shaderWrite = (std::max)(span.stamp.shaderWrite, epoch + 1);
                }
            }
        }
        MergeSpans(history);
    }

    // range 的两个端点已在准备期切开, 所以遍历命中的整段即可更新对应字节的记录.
    template <typename Action>
    static void VisitBuffer(std::vector<BufferAccessSpan> &history, BufferRange range, Action action)
    {
        auto span = std::ranges::upper_bound(history, range.byteOffset, {}, &BufferAccessSpan::end);
        const auto end = range.byteOffset + range.byteSize;
        for (; span != history.end() && span->first < end; ++span)
        {
            action(span->stamp);
        }
    }

    // Texture 的表在创建时按层展开. 使用原存储的层和 mip, 来自不同视图的访问可以命中同一项.
    template <typename Action>
    static void VisitTexture(std::vector<ShaderAccessStamp> &history, std::uint32_t levels,
                             TextureSubresourceSet range, Action action)
    {
        for (auto layer = range.baseArraySlice; layer < range.baseArraySlice + range.numArraySlices; ++layer)
        {
            for (auto mip = range.baseMipLevel; mip < range.baseMipLevel + range.numMipLevels; ++mip)
            {
                action(history[std::size_t(layer) * levels + mip]);
            }
        }
    }

    bool OpenGLCommandList::AddNativeAccess(NativeAccess access, const char *operation)
    {
        try
        {
            m_Packet->accesses.push_back(access);
            return true;
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report(operation, "CPU allocation failed while preparing native access");
            return false;
        }
    }

    bool OpenGLCommandList::PrepareNativeAccesses(const char *operation)
    {
        if (m_Packet->accesses.empty())
        {
            return true;
        }
        if (m_Device->m_AccessEpoch == std::numeric_limits<std::uint64_t>::max())
        {
            m_Device->Report(operation, "CapacityExceeded: native access sequence exhausted");
            return false;
        }
        const auto &accesses = m_Packet->accesses;
        for (auto it = accesses.begin(); it != accesses.end(); ++it)
        {
            auto *buffer = it->buffer;
            // 每次访问至多新增两个切口. 先紧缩每个临近预算的 Buffer, 再为当前命令的所有范围切分.
            if (buffer && buffer->m_ShaderAccess.size() + 2 * accesses.size() > UploadItemLimit &&
                std::ranges::find(accesses.begin(), it, buffer, &NativeAccess::buffer) == it)
            {
                CompactBufferSpans(buffer->m_ShaderAccess, m_Device->m_CompletedAccessEpoch,
                                   m_Device->m_VisibleAccessEpoch);
            }
        }
        GLbitfield barriers = 0;
        bool split = true;
        try
        {
            for (const auto &use : m_Packet->accesses)
            {
                // MemoryBarrier 按 bit 覆盖此前同 context 的命令. 只在本次重叠访问需要该 bit 时发射.
                const auto visible = m_Device->m_VisibleAccessEpoch[std::countr_zero(use.domain)];
                // 已完成的访问可以解除后续写的顺序约束. 读取 Shader 写入仍取决于该消费域的屏障进度.
                const auto ordered = (std::max)(visible, m_Device->m_CompletedAccessEpoch);
                const auto examine = [&](const ShaderAccessStamp &stamp)
                {
                    if (stamp.shaderWrite > visible || (use.shaderWrite && stamp.lastAccess > ordered))
                    {
                        barriers |= use.domain;
                    }
                };
                if (use.buffer)
                {
                    auto &history = use.buffer->m_ShaderAccess;
                    split = SplitSpan(history, use.bytes.byteOffset) &&
                            SplitSpan(history, use.bytes.byteOffset + use.bytes.byteSize);
                    if (!split)
                    {
                        break;
                    }
                    VisitBuffer(history, use.bytes, examine);
                }
                else
                {
                    VisitTexture(use.texture->m_ShaderAccess, use.texture->m_Desc.mipLevels,
                                 use.subresources, examine);
                }
            }
        }
        catch (const std::bad_alloc &)
        {
            split = false;
        }
        // 准备中途可能已切出等价的相邻段. 合并回去后报告失败, 此次候选没有写入历史.
        if (!split)
        {
            for (const auto &use : m_Packet->accesses)
            {
                if (use.buffer)
                {
                    MergeSpans(use.buffer->m_ShaderAccess);
                }
            }
            m_Device->Report(operation, "CapacityExceeded: could not represent shader access intervals");
            return false;
        }
        if (barriers != 0)
        {
            glMemoryBarrier(barriers);
            ++m_Device->m_CommandCounts.barriers;
            if (!m_Device->CheckNative("MemoryBarrier"))
            {
                m_Packet->invalid = true;
                return false;
            }
            for (auto bits = barriers; bits != 0; bits &= bits - 1)
            {
                m_Device->m_VisibleAccessEpoch[std::countr_zero(bits)] = m_Device->m_AccessEpoch;
            }
        }
        return true;
    }

    void OpenGLCommandList::RecordNativeAccesses() noexcept
    {
        if (m_Packet->accesses.empty())
        {
            return;
        }
        // 同一命令的所有访问共享序号. 只有 Shader 写或已证明覆盖完整范围的普通写才替换该范围的写入来源.
        const auto epoch = ++m_Device->m_AccessEpoch;
        for (const auto &use : m_Packet->accesses)
        {
            const auto record = [&](ShaderAccessStamp &stamp)
            {
                stamp.lastAccess = epoch;
                if (use.shaderWrite || use.replaces)
                {
                    stamp.shaderWrite = use.shaderWrite ? epoch : 0;
                }
            };
            if (use.buffer)
            {
                VisitBuffer(use.buffer->m_ShaderAccess, use.bytes, record);
            }
            else
            {
                VisitTexture(use.texture->m_ShaderAccess, use.texture->m_Desc.mipLevels, use.subresources, record);
            }
        }
        for (const auto &use : m_Packet->accesses)
        {
            if (use.buffer)
            {
                MergeSpans(use.buffer->m_ShaderAccess);
            }
        }
    }

    void OpenGLCommandList::RecordClearedSubresource(OpenGLTexture &texture, std::uint32_t layer,
                                                     std::uint32_t mip) noexcept
    {
        if (!texture.m_ShaderAccess.empty())
        {
            texture.m_ShaderAccess[std::size_t(layer) * texture.m_Desc.mipLevels + mip] = {0, ++m_Device->m_AccessEpoch};
        }
    }

    void OpenGLCommandList::SetEnableUavBarriersForTexture(Texture *texture, bool enable)
    {
        if (auto *packet = StateRecording("SetEnableUavBarriersForTexture"))
        {
            auto *native = TextureFor(texture, "SetEnableUavBarriersForTexture");
            if (native && native->GetDesc().isUAV && Retain(native, native->m_InternalOwners))
            {
                packet->tracker.SetEnableUavBarriersForTexture(&native->m_State, enable);
                return;
            }
            m_Device->Report("SetEnableUavBarriersForTexture", "InvalidArgument: a retained storage image is required");
        }
    }

    void OpenGLCommandList::SetEnableUavBarriersForBuffer(Buffer *buffer, bool enable)
    {
        if (auto *packet = StateRecording("SetEnableUavBarriersForBuffer"))
        {
            auto *native = BufferFor(buffer, "SetEnableUavBarriersForBuffer");
            if (native && native->GetDesc().canHaveUAVs && Retain(native, native->m_InternalOwners))
            {
                packet->tracker.SetEnableUavBarriersForBuffer(&native->m_State, enable);
                return;
            }
            m_Device->Report("SetEnableUavBarriersForBuffer", "InvalidArgument: a retained storage buffer is required");
        }
    }
} // namespace Physara::RHI::OpenGL
