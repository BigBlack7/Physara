#include "CommandList.hpp"

/// @file
/// @brief 实现 typed clear, 普通资源复制和两类颜色 blit. 普通复制保留存储编码, mip 生成和 resolve 各按附件操作.

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

#include "../Common/Transfer.hpp"
#include "Staging.hpp"

namespace Physara::RHI::OpenGL
{
    bool OpenGLCommandList::TransferRange(OpenGLTexture &texture, TextureSubresourceSet requested,
                                          TextureSubresourceSet &resolved, const char *operation)
    {
        const auto &desc = texture.GetDesc();
        if (!Transfer::ExactRange(desc, requested))
        {
            m_Device->Report(operation, "InvalidArgument: explicit texture subresource range");
            return false;
        }
        resolved = requested.Resolve(desc, false);
        return true;
    }

    // 为 clear 或 blit 选择单个附件. 只有 blit 源需要可读的颜色附件.
    static void AttachClear(GLuint fbo, OpenGLTexture &texture, GLuint mip, GLuint layer, bool depth,
                            bool read = false) noexcept
    {
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, 0, 0);
        glNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
        const auto &desc = texture.GetDesc();
        const auto point = !depth                                  ? GL_COLOR_ATTACHMENT0
                           : GetFormatInfo(desc.format).hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                   : GL_DEPTH_ATTACHMENT;
        if (desc.dimension == TextureDimension::Texture2D || desc.dimension == TextureDimension::Texture2DMS)
        {
            glNamedFramebufferTexture(fbo, point, texture.Name(), GLint(mip));
        }
        else
        {
            glNamedFramebufferTextureLayer(fbo, point, texture.Name(), GLint(mip), GLint(layer));
        }
        glNamedFramebufferDrawBuffer(fbo, depth ? GL_NONE : GL_COLOR_ATTACHMENT0);
        glNamedFramebufferReadBuffer(fbo, read ? GL_COLOR_ATTACHMENT0 : GL_NONE);
    }

    // 清除结束立即解除颜色及两个 depth/stencil attachment point, 辅助 FBO 不跨命令拥有纹理.
    static void DetachClear(GLuint fbo) noexcept
    {
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, 0, 0);
        glNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
    }

    void OpenGLCommandList::ClearTextureFloat(Texture *texture, TextureSubresourceSet range, const Color &color)
    {
        ClearColorTexture(texture, range, color, 0, false);
    }

    void OpenGLCommandList::ClearTextureUInt(Texture *texture, TextureSubresourceSet range, std::uint32_t value)
    {
        ClearColorTexture(texture, range, {}, value, true);
    }

    void OpenGLCommandList::ClearColorTexture(Texture *texture, TextureSubresourceSet requested, const Color &color,
                                              std::uint32_t value, bool integer)
    {
        // 先证明整组选层与数值类别, 后续循环仅物化同一已验证请求, 不边清边裁剪非法范围.
        const auto *operation = integer ? "ClearTextureUInt" : "ClearTextureFloat";
        if (!Recording(operation))
        {
            return;
        }
        auto *native = TextureFor(texture, operation);
        TextureSubresourceSet range;
        if (!native || !TransferRange(*native, requested, range, operation))
        {
            return;
        }
        const auto &desc = native->GetDesc();
        const auto &format = GetFormatInfo(desc.format);
        const auto support = m_Device->GetTransferFormatSupport(desc.format, desc.dimension);
        if (!desc.isRenderTarget || format.hasDepth || !(support.colorClearSamples & (1u << desc.sampleCount)) ||
            (format.kind == FormatKind::Integer) != integer)
        {
            m_Device->Report(operation, "Unsupported or InvalidArgument: typed render-target clear");
            return;
        }
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, range, TextureAspect::Color, ResourceStates::CopyDest, operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_FRAMEBUFFER_BARRIER_BIT) ||
            !m_Device->BeginNative(operation) || !PrepareNativeAccesses(operation))
        {
            return;
        }
        auto &context = m_Device->m_ContextState;
        context.ApplyClearState(format.isSRGB);
        const auto fbo = context.ClearFramebuffer();
        // 显式绑定辅助 FBO. 从深度独占目标切换时, 非绑定 DSA clear 在实测驱动上漏清颜色区域.
        // 绑定走 ContextState, 后续 Draw 根据逻辑 GraphicsState 恢复目标.
        context.ApplyFramebuffer(fbo);
        const std::array<GLfloat, 4> floats{color.r, color.g, color.b, color.a};
        const std::array<GLuint, 4> uints{value, value, value, value};
        // 有符号清除按相同 32-bit 载荷解释, 不把 UInt 的高位值经浮点或数值窄化转换.
        const auto signedValue = std::bit_cast<GLint>(value);
        const std::array<GLint, 4> ints{signedValue, signedValue, signedValue, signedValue};
        m_Packet->issuedWork = true;
        for (auto layer = range.baseArraySlice; layer < range.baseArraySlice + range.numArraySlices; ++layer)
        {
            for (auto mip = range.baseMipLevel; mip < range.baseMipLevel + range.numMipLevels; ++mip)
            {
                AttachClear(fbo, *native, mip, layer, false);
                if (integer && format.isSigned)
                {
                    glClearNamedFramebufferiv(fbo, GL_COLOR, 0, ints.data());
                }
                else if (integer)
                {
                    glClearNamedFramebufferuiv(fbo, GL_COLOR, 0, uints.data());
                }
                else
                {
                    glClearNamedFramebufferfv(fbo, GL_COLOR, 0, floats.data());
                }
                if (!native->m_ShaderAccess.empty())
                {
                    if (!m_Device->CheckNative(operation, native->Name()))
                    {
                        DetachClear(fbo);
                        m_Packet->invalid = true;
                        context.Invalidate();
                        return;
                    }
                    // 逐个子资源登记已完成的完整覆写, 后面的层或 mip 即使失败也保留当前前缀.
                    RecordClearedSubresource(*native, layer, mip);
                }
            }
        }
        DetachClear(fbo);
        if (!m_Device->CheckNative(operation, native->Name()))
        {
            m_Packet->invalid = true;
            context.Invalidate();
        }
    }

    void OpenGLCommandList::ClearDepthStencilTexture(Texture *texture, TextureSubresourceSet requested, bool clearDepth,
                                                     float depth, bool clearStencil, uint8_t stencil)
    {
        // 无 aspect 请求是空操作; 单 aspect 使用对应 typed clear, 保留另一 aspect 的原始内容.
        constexpr auto Operation = "ClearDepthStencilTexture";
        if (!Recording(Operation) || (!clearDepth && !clearStencil))
        {
            return;
        }
        auto *native = TextureFor(texture, Operation);
        TextureSubresourceSet range;
        if (!native || !TransferRange(*native, requested, range, Operation))
        {
            return;
        }
        const auto &desc = native->GetDesc();
        const auto &format = GetFormatInfo(desc.format);
        if (!desc.isRenderTarget || !m_Device->GetTransferFormatSupport(desc.format, desc.dimension).depthStencilClear ||
            (clearStencil && !format.hasStencil) ||
            (clearDepth && (!std::isfinite(depth) || depth < 0.f || depth > 1.f)))
        {
            m_Device->Report(Operation, "InvalidArgument: depth/stencil format, aspect or clear value");
            return;
        }
        const auto aspects = clearDepth && clearStencil ? TextureAspect::DepthStencil
                           : clearDepth ? TextureAspect::Depth : TextureAspect::Stencil;
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, range, aspects, ResourceStates::CopyDest, Operation) ||
            !m_Device->BeginNative(Operation))
        {
            return;
        }
        auto &context = m_Device->m_ContextState;
        context.ApplyClearState(false);
        const auto fbo = context.ClearFramebuffer();
        // 与颜色清除使用同一明确的 FBO 切换, 不沿用上一 Draw 的目标.
        context.ApplyFramebuffer(fbo);
        const GLint stencilValue = stencil;
        m_Packet->issuedWork = true;
        for (auto layer = range.baseArraySlice; layer < range.baseArraySlice + range.numArraySlices; ++layer)
        {
            for (auto mip = range.baseMipLevel; mip < range.baseMipLevel + range.numMipLevels; ++mip)
            {
                AttachClear(fbo, *native, mip, layer, true);
                if (clearDepth && clearStencil)
                {
                    glClearNamedFramebufferfi(fbo, GL_DEPTH_STENCIL, 0, depth, stencilValue);
                }
                else if (clearDepth)
                {
                    glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &depth);
                }
                else
                {
                    glClearNamedFramebufferiv(fbo, GL_STENCIL, 0, &stencilValue);
                }
            }
        }
        DetachClear(fbo);
        if (!m_Device->CheckNative(Operation, native->Name()))
        {
            m_Packet->invalid = true;
            context.Invalidate();
            return;
        }
        if (clearDepth)
        {
            // 用实际 clear 范围推进深度写入历史. 仅清 stencil 时保留深度采样依赖的进度.
            RecordDepthWrite(*native, range);
        }
    }

    /// @brief Buffer 区间先用减法核对容量及 native 窄化, 空区间仍核对边界.
    static bool BufferInterval(const OpenGLBuffer &buffer, std::uint64_t offset, std::uint64_t size) noexcept
    {
        return offset <= buffer.GetDesc().byteSize && size <= buffer.GetDesc().byteSize - offset &&
               offset <= std::uint64_t(std::numeric_limits<GLintptr>::max()) &&
               size <= std::uint64_t(std::numeric_limits<GLsizeiptr>::max());
    }

    void OpenGLCommandList::ClearBufferUInt(Buffer *buffer, uint32_t value)
    {
        constexpr auto Operation = "ClearBufferUInt";
        if (!Recording(Operation))
        {
            return;
        }
        auto *native = BufferFor(buffer, Operation);
        if (!native)
        {
            return;
        }
        const auto &desc = native->GetDesc();
        if (!desc.canHaveUAVs || (desc.byteSize % sizeof(value)) != 0)
        {
            m_Device->Report(Operation, "InvalidArgument: clear requires four-byte aligned ordinary UAV storage");
            return;
        }
        if (!Retain(native, native->m_InternalOwners) ||
            !UseBuffer(*native, ResourceStates::CopyDest, Operation, BufferRange{0, desc.byteSize},
                       false, StorageAccess::None, GL_BUFFER_UPDATE_BARRIER_BIT, true) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return;
        }
        m_Packet->issuedWork = true;
        glClearNamedBufferData(native->Name(), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);
        if (m_Device->CheckNative(Operation, native->Name()))
        {
            RecordNativeAccesses();
        }
        else
        {
            m_Packet->invalid = true;
        }
    }

    void OpenGLCommandList::CopyBuffer(Buffer *destination, uint64_t destinationOffset, Buffer *source,
                                       uint64_t sourceOffset, uint64_t size)
    {
        // 区间和同 storage 重叠先检查, 空 copy 不取得 lease 或伪造已发工作.
        constexpr auto Operation = "CopyBuffer";
        if (!Recording(Operation))
        {
            return;
        }
        auto *dest = BufferFor(destination, Operation);
        auto *src = BufferFor(source, Operation);
        if (!dest || !src)
        {
            return;
        }
        // 普通 CopyBuffer 使用固定物理地址, 而 volatile 的逻辑偏移 0 随 recording 选择版本.
        if (dest->GetDesc().isVolatile || src->GetDesc().isVolatile)
        {
            m_Device->Report(Operation, "Unsupported: copying volatile constant buffers");
            return;
        }
        const auto srcMode = src->GetDesc().cpuAccess;
        const auto destMode = dest->GetDesc().cpuAccess;
        const bool fromHost = srcMode == CpuAccessMode::Write;
        const bool toHost = destMode == CpuAccessMode::Read;
        if (srcMode == CpuAccessMode::Read || destMode == CpuAccessMode::Write || (fromHost && toHost) ||
            ((fromHost || toHost) && m_Packet->useMode == UseMode::GraphManaged))
        {
            m_Device->Report(Operation, "Unsupported: staging direction or graph buffer endpoint");
            return;
        }
        if (!BufferInterval(*dest, destinationOffset, size) || !BufferInterval(*src, sourceOffset, size) ||
            (dest == src && size != 0 && destinationOffset < sourceOffset + size &&
             sourceOffset < destinationOffset + size))
        {
            m_Device->Report(Operation, "InvalidArgument: buffer copy interval or overlapping storage");
            return;
        }
        if (size == 0)
        {
            return;
        }
        if (!Retain(src, src->m_InternalOwners) || !Retain(dest, dest->m_InternalOwners))
        {
            return;
        }
        auto *staging = fromHost ? src : toHost ? dest : nullptr;
        auto *lease = staging ? PrepareStagingUse(staging->m_Staging, staging->m_StagingReady,
                                                   staging->m_Desc.cpuAccess, Operation) : nullptr;
        // staging 的专用端点由方向和物理包保护. 普通端点仍走精确 tracker 和实际消费域.
        if ((staging && !lease) ||
            (!fromHost && !UseBuffer(*src, ResourceStates::CopySource, Operation, BufferRange{sourceOffset, size},
                                    false, StorageAccess::None, GL_BUFFER_UPDATE_BARRIER_BIT)) ||
            (!toHost && !UseBuffer(*dest, ResourceStates::CopyDest, Operation, BufferRange{destinationOffset, size},
                                  false, StorageAccess::None, GL_BUFFER_UPDATE_BARRIER_BIT, true)) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return;
        }
        if (lease)
        {
            RecordStagingUse(*lease);
        }
        m_Packet->issuedWork = true;
        glCopyNamedBufferSubData(src->Name(), dest->Name(), GLintptr(sourceOffset), GLintptr(destinationOffset),
                                 GLsizeiptr(size));
        if (m_Device->CheckNative(Operation, dest->Name()))
        {
            RecordNativeAccesses();
        }
        else
        {
            m_Packet->invalid = true;
        }
    }

    void OpenGLCommandList::CopyTexture(Texture *destination, const TextureSlice &destinationSlice, Texture *source,
                                        const TextureSlice &sourceSlice)
    {
        // 只复制原始 texel/编码, 空间区域与 array/cube 层分别解释, 不借助 blit 引入缩放或转色.
        constexpr auto Operation = "CopyTexture";
        if (!Recording(Operation))
        {
            return;
        }
        auto *dest = TextureFor(destination, Operation);
        auto *src = TextureFor(source, Operation);
        if (!dest || !src)
        {
            return;
        }
        TextureSlice d, s;
        const auto &dd = dest->GetDesc();
        const auto &sd = src->GetDesc();
        if (dd.format != sd.format || !m_Device->GetTransferFormatSupport(dd.format, dd.dimension).copy ||
            !m_Device->GetTransferFormatSupport(sd.format, sd.dimension).copy ||
            !ResolvePlanarSlice(dd, destinationSlice, d) || !ResolvePlanarSlice(sd, sourceSlice, s) ||
            d.width != s.width || d.height != s.height ||
            (dest == src && Transfer::PlanarSlicesOverlap(d, s)))
        {
            m_Device->Report(Operation, "Unsupported or InvalidArgument: texture copy profile, region or overlap");
            return;
        }
        // 同格式复制消费实际存储的全部分量, packed 复制需要两端同时提供深度和模板权限.
        const auto aspects = FormatAspects(GetFormatInfo(sd.format));
        // 纹理历史按整个 mip/layer 跟踪. 只复制部分矩形时要保留目标其余像素的 Shader 写入来源.
        const bool whole = Transfer::WholeSlice(dd, d);
        if (!Retain(src, src->m_InternalOwners) || !Retain(dest, dest->m_InternalOwners) ||
            !UseTexture(*src, {s.mipLevel, 1, s.arraySlice, 1}, aspects, ResourceStates::CopySource, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_TEXTURE_UPDATE_BARRIER_BIT) ||
            !UseTexture(*dest, {d.mipLevel, 1, d.arraySlice, 1}, aspects, ResourceStates::CopyDest, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_TEXTURE_UPDATE_BARRIER_BIT, whole) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return;
        }
        m_Packet->issuedWork = true;
        glCopyImageSubData(src->Name(), TextureTarget(sd.dimension), GLint(s.mipLevel), GLint(s.x), GLint(s.y),
                           GLint(s.arraySlice), dest->Name(), TextureTarget(dd.dimension), GLint(d.mipLevel),
                           GLint(d.x), GLint(d.y), GLint(d.arraySlice), GLsizei(s.width), GLsizei(s.height), 1);
        if (m_Device->CheckNative(Operation, dest->Name()))
        {
            RecordNativeAccesses();
        }
        else
        {
            m_Packet->invalid = true;
        }
    }

    // 每次选择实际的单层 attachment. FBO 不跨调用持有纹理, 已发命令由包中的原存储引用保活.
    bool OpenGLCommandList::BlitColor(OpenGLTexture &dest, std::uint32_t destMip, std::uint32_t destLayer,
                                     OpenGLTexture &src, std::uint32_t srcMip, std::uint32_t srcLayer,
                                     GLenum filter, const char *operation, std::uint64_t &epoch)
    {
        auto &context = m_Device->m_ContextState;
        const GLuint readFbo = context.ReadTransferFramebuffer();
        const GLuint writeFbo = context.ClearFramebuffer();
        const auto &d = dest.GetDesc();
        const auto &s = src.GetDesc();
        AttachClear(readFbo, src, srcMip, srcLayer, false, true);
        AttachClear(writeFbo, dest, destMip, destLayer, false);
        m_Packet->issuedWork = true;
        glBlitNamedFramebuffer(readFbo, writeFbo, 0, 0, GLint(Transfer::MipSize(s.width, srcMip)),
                               GLint(Transfer::MipSize(s.height, srcMip)), 0, 0,
                               GLint(Transfer::MipSize(d.width, destMip)), GLint(Transfer::MipSize(d.height, destMip)),
                               GL_COLOR_BUFFER_BIT, filter);
        DetachClear(readFbo);
        DetachClear(writeFbo);
        if (!m_Device->CheckNative(operation, dest.Name()))
        {
            m_Packet->invalid = true;
            context.Invalidate();
            return false;
        }
        // 保留已发前缀的访问. 同一 blit 序列共享序号, 层间的顺序由普通 framebuffer 操作提供.
        if (epoch == 0 && (!src.m_ShaderAccess.empty() || !dest.m_ShaderAccess.empty()))
        {
            epoch = ++m_Device->m_AccessEpoch;
        }
        if (!src.m_ShaderAccess.empty())
        {
            src.m_ShaderAccess[std::size_t(srcLayer) * s.mipLevels + srcMip].lastAccess = epoch;
        }
        if (!dest.m_ShaderAccess.empty())
        {
            dest.m_ShaderAccess[std::size_t(destLayer) * d.mipLevels + destMip] = {0, epoch};
        }
        return true;
    }

    bool OpenGLCommandList::GenerateMipmaps(Texture *texture, TextureSubresourceSet requested)
    {
        constexpr auto Operation = "GenerateMipmaps";
        if (!Recording(Operation))
        {
            return false;
        }
        auto *native = TextureFor(texture, Operation);
        TextureSubresourceSet range;
        if (!native || !TransferRange(*native, requested, range, Operation))
        {
            return false;
        }
        const auto &desc = native->GetDesc();
        if (!desc.isRenderTarget || !m_Device->GetTransferFormatSupport(desc.format, desc.dimension).mipmaps ||
            m_Packet->useMode == UseMode::GraphManaged)
        {
            m_Device->Report(Operation, "Unsupported: mip generation requires a filterable color attachment and direct use");
            return false;
        }
        if (range.numMipLevels == 1)
        {
            return true;
        }
        // 先为整条链准备外部用途. 每层的第一次 blit 读基础级, 后面的 blit 读取刚写完的前一级.
        auto source = range;
        source.numMipLevels = 1;
        auto dest = range;
        ++dest.baseMipLevel;
        --dest.numMipLevels;
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, source, TextureAspect::Color, ResourceStates::CopySource, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_FRAMEBUFFER_BARRIER_BIT) ||
            !UseTexture(*native, dest, TextureAspect::Color, ResourceStates::CopyDest, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_FRAMEBUFFER_BARRIER_BIT, true) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return false;
        }
        m_Device->m_ContextState.ApplyBlitState(GetFormatInfo(desc.format).isSRGB);
        std::uint64_t epoch = 0;
        for (auto layer = range.baseArraySlice; layer < range.baseArraySlice + range.numArraySlices; ++layer)
        {
            for (auto mip = dest.baseMipLevel; mip < dest.baseMipLevel + dest.numMipLevels; ++mip)
            {
                if (!BlitColor(*native, mip, layer, *native, mip - 1, layer, GL_LINEAR, Operation, epoch))
                {
                    return false;
                }
            }
        }
        return true;
    }

    void OpenGLCommandList::ResolveTexture(Texture *destination, const TextureSubresourceSet &requestedDest,
                                           Texture *source, const TextureSubresourceSet &requestedSrc)
    {
        constexpr auto Operation = "ResolveTexture";
        if (!Recording(Operation))
        {
            return;
        }
        auto *dest = TextureFor(destination, Operation);
        auto *src = TextureFor(source, Operation);
        TextureSubresourceSet dstRange, srcRange;
        if (!dest || !src || !TransferRange(*dest, requestedDest, dstRange, Operation) ||
            !TransferRange(*src, requestedSrc, srcRange, Operation))
        {
            return;
        }
        const auto &d = dest->GetDesc();
        const auto &s = src->GetDesc();
        const auto srcSupport = m_Device->GetTransferFormatSupport(s.format, s.dimension);
        const auto dstSupport = m_Device->GetTransferFormatSupport(d.format, d.dimension);
        if (d.format != s.format || !d.isRenderTarget || !s.isRenderTarget ||
            !Transfer::ResolveTargets(d.dimension, s.dimension) || !(srcSupport.resolveSourceSamples & (1u << s.sampleCount)) ||
            !dstSupport.resolveDestination || srcRange.numMipLevels != 1 || dstRange.numMipLevels != 1 ||
            srcRange.numArraySlices != dstRange.numArraySlices ||
            s.width != Transfer::MipSize(d.width, dstRange.baseMipLevel) ||
            s.height != Transfer::MipSize(d.height, dstRange.baseMipLevel) || m_Packet->useMode == UseMode::GraphManaged)
        {
            m_Device->Report(Operation, "Unsupported or InvalidArgument: color format, samples or resolve pairing");
            return;
        }
        if (!Retain(src, src->m_InternalOwners) || !Retain(dest, dest->m_InternalOwners) ||
            !UseTexture(*src, srcRange, TextureAspect::Color, ResourceStates::ResolveSource, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_FRAMEBUFFER_BARRIER_BIT) ||
            !UseTexture(*dest, dstRange, TextureAspect::Color, ResourceStates::ResolveDest, Operation,
                        ResourceStates::Unknown, StorageAccess::None, GL_FRAMEBUFFER_BARRIER_BIT, true) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return;
        }
        // 两端按各自的 baseArraySlice 配对. 用 NEAREST 触发等尺寸颜色 resolve, 样本归约由驱动执行.
        m_Device->m_ContextState.ApplyBlitState(false);
        std::uint64_t epoch = 0;
        for (std::uint32_t layer = 0; layer < srcRange.numArraySlices; ++layer)
        {
            if (!BlitColor(*dest, dstRange.baseMipLevel, dstRange.baseArraySlice + layer, *src, srcRange.baseMipLevel,
                           srcRange.baseArraySlice + layer, GL_NEAREST, Operation, epoch))
            {
                return;
            }
        }
    }
} // namespace Physara::RHI::OpenGL
