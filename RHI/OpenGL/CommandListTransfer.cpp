#include "CommandList.hpp"

/// @file
/// @brief 完整二维子资源 typed clear 与精确区间 copy, 先预检/保活/建立用途再发出内容操作.

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace Physara::RHI::OpenGL
{
    /// @brief 当前二维传输 profile 包含普通二维 storage 与数组/cube 的单层, 不扩展 3D/MS 支持.
    static bool Planar(const TextureDesc &desc) noexcept
    {
        return desc.sampleCount == 1 &&
               (desc.dimension == TextureDimension::Texture2D || desc.dimension == TextureDimension::Texture2DArray ||
                desc.dimension == TextureDimension::TextureCube ||
                desc.dimension == TextureDimension::TextureCubeArray);
    }

    bool OpenGLCommandList::TransferRange(OpenGLTexture &texture, TextureSubresourceSet requested,
                                          TextureSubresourceSet &resolved, const char *operation)
    {
        const auto &desc = texture.GetDesc();
        if (!Planar(desc))
        {
            m_Device->Report(operation, "Unsupported: single-sample 2D subresource required");
            return false;
        }
        const auto layers = desc.arraySize;
        if (requested.baseMipLevel >= desc.mipLevels || requested.baseArraySlice >= layers ||
            requested.numMipLevels == 0 || requested.numArraySlices == 0 ||
            (requested.numMipLevels != TextureSubresourceSet::AllMipLevels &&
             requested.numMipLevels > desc.mipLevels - requested.baseMipLevel) ||
            (requested.numArraySlices != TextureSubresourceSet::AllArraySlices &&
             requested.numArraySlices > layers - requested.baseArraySlice))
        {
            m_Device->Report(operation, "InvalidArgument: explicit texture subresource range");
            return false;
        }
        resolved = requested.Resolve(desc, false);
        return true;
    }

    /// @brief 清除辅助 FBO 不缓存附件, 每次显式选层并在命令后解除, 避免长期保留 storage.
    static void AttachClear(GLuint fbo, OpenGLTexture &texture, GLuint mip, GLuint layer, bool depth) noexcept
    {
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, 0, 0);
        glNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
        const auto &desc = texture.GetDesc();
        const auto point = !depth                                  ? GL_COLOR_ATTACHMENT0
                           : GetFormatInfo(desc.format).hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                   : GL_DEPTH_ATTACHMENT;
        if (desc.dimension == TextureDimension::Texture2D)
        {
            glNamedFramebufferTexture(fbo, point, texture.Name(), GLint(mip));
        }
        else
        {
            glNamedFramebufferTextureLayer(fbo, point, texture.Name(), GLint(mip), GLint(layer));
        }
        glNamedFramebufferDrawBuffer(fbo, depth ? GL_NONE : GL_COLOR_ATTACHMENT0);
        glNamedFramebufferReadBuffer(fbo, GL_NONE);
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
        if (!desc.isRenderTarget || format.hasDepth || (format.kind == FormatKind::Integer) != integer)
        {
            m_Device->Report(operation, "Unsupported or InvalidArgument: typed render-target clear");
            return;
        }
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, range, ResourceStates::CopyDest, operation) || !m_Device->BeginNative(operation))
        {
            return;
        }
        auto &context = m_Device->m_ContextState;
        context.ApplyClearState(format.isSRGB);
        const auto fbo = context.ClearFramebuffer();
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
        if (!desc.isRenderTarget || !format.hasDepth || (clearStencil && !format.hasStencil) ||
            (clearDepth && (!std::isfinite(depth) || depth < 0.f || depth > 1.f)))
        {
            m_Device->Report(Operation, "InvalidArgument: depth/stencil format, aspect or clear value");
            return;
        }
        if (!Retain(native, native->m_InternalOwners) ||
            !UseTexture(*native, range, ResourceStates::CopyDest, Operation) || !m_Device->BeginNative(Operation))
        {
            return;
        }
        auto &context = m_Device->m_ContextState;
        context.ApplyClearState(false);
        const auto fbo = context.ClearFramebuffer();
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
        }
    }

    /// @brief Buffer 区间先用减法核对容量及 native 窄化, 空区间仍核对边界.
    static bool BufferInterval(const OpenGLBuffer &buffer, std::uint64_t offset, std::uint64_t size) noexcept
    {
        return offset <= buffer.GetDesc().byteSize && size <= buffer.GetDesc().byteSize - offset &&
               offset <= std::uint64_t(std::numeric_limits<GLintptr>::max()) &&
               size <= std::uint64_t(std::numeric_limits<GLsizeiptr>::max());
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
        if (!Retain(src, src->m_InternalOwners) || !Retain(dest, dest->m_InternalOwners) ||
            !UseBuffer(*src, ResourceStates::CopySource, Operation) ||
            !UseBuffer(*dest, ResourceStates::CopyDest, Operation) || !m_Device->BeginNative(Operation))
        {
            return;
        }
        m_Packet->issuedWork = true;
        glCopyNamedBufferSubData(src->Name(), dest->Name(), GLintptr(sourceOffset), GLintptr(destinationOffset),
                                 GLsizeiptr(size));
        m_Packet->invalid = !m_Device->CheckNative(Operation, dest->Name());
    }

    /// @brief 先核对 mip/layer 再调用公共 Resolve, 全尺寸哨兵沿公共语义而非隐式剩余大小.
    static bool ResolveSlice(const TextureDesc &desc, const TextureSlice &input, TextureSlice &out) noexcept
    {
        if (!Planar(desc) || input.mipLevel >= desc.mipLevels || input.arraySlice >= desc.arraySize)
        {
            return false;
        }
        out = input.Resolve(desc);
        const auto width = std::max(1u, desc.width >> out.mipLevel);
        const auto height = std::max(1u, desc.height >> out.mipLevel);
        return out.z == 0 && out.depth == 1 && out.width != 0 && out.height != 0 && out.x <= width &&
               out.width <= width - out.x && out.y <= height && out.height <= height - out.y;
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
        if (dd.format != sd.format || !ResolveSlice(dd, destinationSlice, d) || !ResolveSlice(sd, sourceSlice, s) ||
            d.width != s.width || d.height != s.height ||
            (dest == src && d.mipLevel == s.mipLevel && d.arraySlice == s.arraySlice && d.x < s.x + s.width &&
             s.x < d.x + d.width && d.y < s.y + s.height && s.y < d.y + d.height))
        {
            m_Device->Report(Operation, "Unsupported or InvalidArgument: texture copy profile, region or overlap");
            return;
        }
        if (!Retain(src, src->m_InternalOwners) || !Retain(dest, dest->m_InternalOwners) ||
            !UseTexture(*src, {s.mipLevel, 1, s.arraySlice, 1}, ResourceStates::CopySource, Operation) ||
            !UseTexture(*dest, {d.mipLevel, 1, d.arraySlice, 1}, ResourceStates::CopyDest, Operation) ||
            !m_Device->BeginNative(Operation))
        {
            return;
        }
        m_Packet->issuedWork = true;
        glCopyImageSubData(src->Name(), TextureTarget(sd.dimension), GLint(s.mipLevel), GLint(s.x), GLint(s.y),
                           GLint(s.arraySlice), dest->Name(), TextureTarget(dd.dimension), GLint(d.mipLevel),
                           GLint(d.x), GLint(d.y), GLint(d.arraySlice), GLsizei(s.width), GLsizei(s.height), 1);
        m_Packet->invalid = !m_Device->CheckNative(Operation, dest->Name());
    }
} // namespace Physara::RHI::OpenGL
