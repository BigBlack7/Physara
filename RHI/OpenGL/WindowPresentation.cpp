#include "Backend.hpp"

/// @file
/// @brief 关闭列表后执行等尺寸窗口颜色复制. 先核对物理包和导出状态, 再登记源访问依赖.
/// ContextState 建立默认后缓冲的复制条件. 后续 Execute 的 fence 覆盖本次源访问.

#include "CommandList.hpp"

namespace Physara::RHI::OpenGL
{
    bool OpenGLDevice::PrepareWindowPresentation(CommandList *commands, Framebuffer *source, bool srgb)
    {
        if (!RequireContext("PrepareWindowPresentation"))
        {
            return false;
        }
        // 包装层的 Closed 表示调用顺序. 此处核对实际 owning 包, 可提交状态和源 FBO 归属.
        auto *list = dynamic_cast<OpenGLCommandList *>(commands);
        auto *framebuffer = dynamic_cast<OpenGLFramebuffer *>(source);
        if (!list || list->m_Device.Get() != this || !framebuffer || framebuffer->Owner() != this ||
            m_ActiveRecording.load() != 0 || !list->m_Packet ||
            list->m_Packet->phase != PacketPhase::Closed || list->m_Packet->invalid ||
            list->m_Packet->tracker.HasErrors() || list->m_Packet->orphaned.load())
        {
            Report("PrepareWindowPresentation", "InvalidState: owning closed Graphics packet and framebuffer required");
            return false;
        }
        // 复制直接消费原生 FBO 的颜色附件 0. 只接受基线 PresentColor 的格式和子资源范围.
        const auto &desc = framebuffer->GetDesc();
        auto *texture = desc.colorAttachments.size() == 1
                            ? dynamic_cast<OpenGLTexture *>(desc.colorAttachments[0].texture)
                            : nullptr;
        const auto range = desc.colorAttachments.empty() ? TextureSubresourceSet{}
                                                         : desc.colorAttachments[0].subresources;
        if (!texture || desc.depthAttachment.texture || texture->GetDesc().dimension != TextureDimension::Texture2D ||
            texture->GetDesc().format != Format::SRGBA8_UNORM || texture->GetDesc().mipLevels != 1 ||
            texture->GetDesc().sampleCount != 1 || range.baseMipLevel != 0 || range.baseArraySlice != 0 ||
            range.numMipLevels != 1 || range.numArraySlices != 1 ||
            framebuffer->GetFramebufferInfo().colorFormats[0] != Format::SRGBA8_UNORM)
        {
            Report("PrepareWindowPresentation", "InvalidArgument: one SRGBA8_UNORM mip/layer required");
            return false;
        }
        // ShaderResource 是 Close 导出的公共状态. 本帧有效颜色由上层录制和同步诊断确认.
        const auto state = list->m_Packet->tracker.GetTextureSubresourceState(&texture->m_State, 0, 0);
        if ((state & ResourceStates::ShaderResource) == ResourceStates::Unknown ||
            !list->m_Packet->tracker.GetTextureBarriers().empty())
        {
            Report("PrepareWindowPresentation", "InvalidState: committed ShaderResource export required");
            return false;
        }
        // 所有可能失败的 CPU 保活在原生访问之前完成. 即使只有 clear, 末端 FBO 也独立进入包.
        if (!list->RetainResource(framebuffer) || !BeginNative("PrepareWindowPresentation"))
        {
            return false;
        }
        // 按默认附件编码选择一次显示编码. sRGB 路径解码源并在目标编码, 线性路径传送存储值.
        m_ContextState.ApplyWindowPresentation(srgb);
        if (!CheckNative("PrepareWindowPresentation.State", framebuffer->Name()))
        {
            // 原生设置可能只应用了前缀. 撤销缓存已知性并将包交由退出路径处理.
            m_ContextState.Invalidate();
            list->m_Packet->invalid = true;
            return false;
        }
        const auto &info = framebuffer->GetFramebufferInfo();
        // 在调用驱动前标记可能已发工作, 使带错返回也保留本次源访问的 lease.
        list->m_Packet->issuedWork = true;
        // 离屏 row 0 是逻辑顶行, 默认窗口原生顶行位于高 y. 仅在此边界反向目的矩形.
        glBlitNamedFramebuffer(framebuffer->Name(), 0, 0, 0, GLsizei(info.width), GLsizei(info.height),
                               0, GLsizei(info.height), GLsizei(info.width), 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        if (!CheckNative("PrepareWindowPresentation.Copy", framebuffer->Name()))
        {
            m_ContextState.Invalidate();
            list->m_Packet->invalid = true;
            return false;
        }
        return true;
    }

    // 后端互操作入口只接受原生设备. Platform 在此之前已通过 Validation 借出原生列表.
    bool PrepareWindowPresentation(Device *device, CommandList *commands, Framebuffer *source, bool srgbBackBuffer)
    {
        auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native && native->PrepareWindowPresentation(commands, source, srgbBackBuffer);
    }
}
