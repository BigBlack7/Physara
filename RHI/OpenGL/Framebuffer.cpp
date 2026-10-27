#include "Backend.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <new>
#include <utility>

namespace Physara::RHI::OpenGL
{
    OpenGLFramebuffer::OpenGLFramebuffer(OpenGLDevice *device, const FramebufferDesc &desc)
        : m_Device(device), m_Desc(desc), m_Info(desc),
          m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Framebuffer))
    {
        for (std::size_t index = 0; index < desc.colorAttachments.size(); ++index)
        {
            m_Textures[index] = desc.colorAttachments[index].texture;
        }
        m_Textures[MaxRenderTargets] = desc.depthAttachment.texture;
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        // 先退休附件 owner 的原生对象, 后释放依赖; FIFO 维持 FBO/view 在 storage 之前.
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
        for (auto &texture : m_Textures)
        {
            texture.Reset();
        }
    }

    FramebufferHandle OpenGLDevice::CreateFramebuffer(const FramebufferDesc &desc)
    {
        if (!RequireContext("CreateFramebuffer"))
        {
            return nullptr;
        }
        const auto colorCount = desc.colorAttachments.size();
        if (colorCount > MaxRenderTargets || colorCount > std::size_t(m_Caps.maxColorAttachments) ||
            colorCount > std::size_t(m_Caps.maxDrawBuffers))
        {
            Report("CreateFramebuffer", "InvalidArgument: native color attachment limit");
            return nullptr;
        }
        if ((colorCount == 0 && !desc.depthAttachment.texture) || desc.shadingRateAttachment.texture)
        {
            Report("CreateFramebuffer", "Unsupported: empty or shading-rate framebuffer");
            return nullptr;
        }
        struct AttachmentPlan
        {
            OpenGLTexture *texture = nullptr;
            GLenum attachment = 0;
            GLenum viewTarget = 0;
            GLenum internalFormat = 0;
            Format format = Format::UNKNOWN;
            GLint level = 0;
            GLint layer = 0;
            bool layeredStorage = false;
            bool reinterpret = false;
        };
        std::array<AttachmentPlan, MaxRenderTargets + 1> plans{};
        std::size_t planCount = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t samples = 0;
        const auto inspect = [&](const FramebufferAttachment &attachment, bool depth, GLenum point)
        {
            auto *texture = dynamic_cast<OpenGLTexture *>(attachment.texture);
            if (!texture || texture->Owner() != this)
            {
                Report("CreateFramebuffer", "InvalidResource: missing, incompatible or foreign texture");
                return false;
            }
            const auto &textureDesc = texture->GetDesc();
            const auto &range = attachment.subresources;
            if (range.numMipLevels != 1 || range.numArraySlices != 1 || range.baseMipLevel >= textureDesc.mipLevels ||
                range.baseArraySlice >= textureDesc.arraySize)
            {
                Report("CreateFramebuffer", "InvalidArgument: exact single mip/layer range",
                       textureDesc.debugName.c_str());
                return false;
            }
            if (textureDesc.dimension == TextureDimension::Texture3D || (!depth && attachment.isReadOnly))
            {
                Report("CreateFramebuffer", "Unsupported: volume attachment or read-only color",
                       textureDesc.debugName.c_str());
                return false;
            }
            const auto format = attachment.format == Format::UNKNOWN ? textureDesc.format : attachment.format;
            const bool reinterpret = format != textureDesc.format;
            const auto viewDimension = textureDesc.dimension == TextureDimension::Texture1D ||
                                               textureDesc.dimension == TextureDimension::Texture1DArray
                                           ? TextureDimension::Texture1D
                                       : textureDesc.sampleCount > 1 ? TextureDimension::Texture2DMS
                                                                     : TextureDimension::Texture2D;
            const auto &storageCaps = GetFormatCaps(textureDesc.format, textureDesc.dimension);
            // 单层 view 的 target 可能与 array/cube storage 不同, 用实际 view target 的能力预检.
            const auto &viewCaps = GetFormatCaps(format, reinterpret ? viewDimension : textureDesc.dimension);
            const auto &formatInfo = GetFormatInfo(format);
            const auto usage = depth ? FormatSupport::DepthStencil : FormatSupport::RenderTarget;
            if (!textureDesc.isRenderTarget || depth != bool(formatInfo.hasDepth) || !(viewCaps.support & usage))
            {
                Report("CreateFramebuffer", "InvalidArgument: attachment format or role",
                       textureDesc.debugName.c_str());
                return false;
            }
            if (reinterpret &&
                (!textureDesc.isTypeless || storageCaps.viewClass == 0 || storageCaps.viewClass != viewCaps.viewClass))
            {
                Report("CreateFramebuffer", "Unsupported: incompatible or typed-only view",
                       textureDesc.debugName.c_str());
                return false;
            }
            const auto mipWidth = std::max(textureDesc.width >> range.baseMipLevel, 1u);
            const auto mipHeight = std::max(textureDesc.height >> range.baseMipLevel, 1u);
            if (planCount != 0 && (width != mipWidth || height != mipHeight || samples != textureDesc.sampleCount))
            {
                Report("CreateFramebuffer", "InvalidArgument: attachment extent or sample mismatch");
                return false;
            }
            width = mipWidth;
            height = mipHeight;
            samples = textureDesc.sampleCount;
            auto &plan = plans[planCount++];
            plan.texture = texture;
            plan.attachment = depth && formatInfo.hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : point;
            plan.format = format;
            plan.internalFormat = viewCaps.internalFormat;
            plan.level = GLint(range.baseMipLevel);
            plan.layer = GLint(range.baseArraySlice);
            plan.reinterpret = reinterpret;
            plan.layeredStorage = textureDesc.dimension == TextureDimension::Texture1DArray ||
                                  textureDesc.dimension == TextureDimension::Texture2DArray ||
                                  textureDesc.dimension == TextureDimension::TextureCube ||
                                  textureDesc.dimension == TextureDimension::TextureCubeArray ||
                                  textureDesc.dimension == TextureDimension::Texture2DMSArray;
            plan.viewTarget = TextureTarget(viewDimension);
            return true;
        };
        for (std::size_t index = 0; index < colorCount; ++index)
        {
            if (!inspect(desc.colorAttachments[index], false, GL_COLOR_ATTACHMENT0 + GLenum(index)))
            {
                return nullptr;
            }
        }
        if (desc.depthAttachment.texture && !inspect(desc.depthAttachment, true, GL_DEPTH_ATTACHMENT))
        {
            return nullptr;
        }
        try
        {
            auto framebuffer = std::make_unique<OpenGLFramebuffer>(this, desc);
            if (desc.depthAttachment.texture)
            {
                framebuffer->m_Info.depthFormat = plans[planCount - 1].format;
            }
            if (!BeginNative("CreateFramebuffer"))
            {
                return nullptr;
            }
            auto &deletion = *framebuffer->m_Deletion;
            glCreateFramebuffers(1, &deletion.name);
            if (!CheckNative("CreateFramebuffer.Name", deletion.name))
            {
                return nullptr;
            }
            if (deletion.name == 0)
            {
                m_Phase = Phase::Faulted;
                Report("CreateFramebuffer", "NativeFailure: zero object name");
                return nullptr;
            }
            for (std::size_t index = 0; index < planCount; ++index)
            {
                const auto &plan = plans[index];
                GLuint attachmentName = plan.texture->Name();
                GLint level = plan.level;
                if (plan.reinterpret)
                {
                    auto &view = deletion.views[deletion.viewCount++];
                    // TextureView 要求尚未取得 target 的名字; CreateTextures 已取得 target, 不能用于此处.
                    glGenTextures(1, &view);
                    if (!CheckNative("CreateFramebuffer.ViewName", view))
                    {
                        return nullptr;
                    }
                    if (view == 0)
                    {
                        m_Phase = Phase::Faulted;
                        Report("CreateFramebuffer", "NativeFailure: zero view name");
                        return nullptr;
                    }
                    glTextureView(view, plan.viewTarget, attachmentName, plan.internalFormat, GLuint(plan.level), 1,
                                  GLuint(plan.layer), 1);
                    if (!CheckNative("CreateFramebuffer.View", view))
                    {
                        return nullptr;
                    }
                    attachmentName = view;
                    level = 0;
                }
                if (plan.layeredStorage && !plan.reinterpret)
                {
                    glNamedFramebufferTextureLayer(deletion.name, plan.attachment, attachmentName, level, plan.layer);
                }
                else
                {
                    glNamedFramebufferTexture(deletion.name, plan.attachment, attachmentName, level);
                }
                if (!CheckNative("CreateFramebuffer.Attachment", deletion.name))
                {
                    return nullptr;
                }
            }
            std::array<GLenum, MaxRenderTargets> drawBuffers{};
            for (std::size_t index = 0; index < colorCount; ++index)
            {
                drawBuffers[index] = GL_COLOR_ATTACHMENT0 + GLenum(index);
            }
            if (colorCount == 0)
            {
                glNamedFramebufferDrawBuffer(deletion.name, GL_NONE);
            }
            else
            {
                glNamedFramebufferDrawBuffers(deletion.name, GLsizei(colorCount), drawBuffers.data());
            }
            if (!CheckNative("CreateFramebuffer.DrawBuffers", deletion.name))
            {
                return nullptr;
            }
            glNamedFramebufferReadBuffer(deletion.name, colorCount == 0 ? GL_NONE : GL_COLOR_ATTACHMENT0);
            if (!CheckNative("CreateFramebuffer.ReadBuffer", deletion.name))
            {
                return nullptr;
            }
            const GLenum status = glCheckNamedFramebufferStatus(deletion.name, GL_FRAMEBUFFER);
            if (!CheckNative("CreateFramebuffer.Completeness", deletion.name))
            {
                return nullptr;
            }
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                char resource[48]{};
                std::snprintf(resource, sizeof(resource), "native-name=%u", deletion.name);
                Report("CreateFramebuffer", "NativeFailure: incomplete framebuffer", resource, status);
                return nullptr;
            }
            framebuffer->Publish();
            return FramebufferHandle::Create(framebuffer.release());
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateFramebuffer", "CPU allocation failed");
            return nullptr;
        }
    }
}
