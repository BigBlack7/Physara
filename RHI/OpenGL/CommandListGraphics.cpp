#include "CommandList.hpp"

/// @file
/// @brief 预检图形候选对象/动态范围/活动接口与 feedback, 完成保活后才应用原生状态并发布.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "InputLayout.hpp"
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief native viewport 采用项目离屏 row 0 约定, 仅检查实际入口会使用的参数及设备限制.
    static bool ValidViewport(const Viewport &v, const DeviceCaps &caps) noexcept
    {
        return std::isfinite(v.minX) && std::isfinite(v.maxX) && std::isfinite(v.minY) && std::isfinite(v.maxY) &&
               std::isfinite(v.minZ) && std::isfinite(v.maxZ) && v.minZ >= 0 && v.minZ <= 1 && v.maxZ >= 0 &&
               v.maxZ <= 1 && v.minX <= v.maxX && v.minY <= v.maxY &&
               double(v.maxX) - v.minX <= caps.maxViewportDimensions[0] &&
               double(v.maxY) - v.minY <= caps.maxViewportDimensions[1] && v.minX >= caps.viewportBounds[0] &&
               v.minX <= caps.viewportBounds[1] && v.minY >= caps.viewportBounds[0] && v.minY <= caps.viewportBounds[1];
    }

    // 允许负坐标交给光栅裁剪, 只拒绝反向区间及 GLsizei 无法表示的区域大小.
    static bool ValidScissor(const Rect &r) noexcept
    {
        const auto width = std::int64_t(r.maxX) - r.minX;
        const auto height = std::int64_t(r.maxY) - r.minY;
        return width >= 0 && height >= 0 && width <= std::numeric_limits<GLsizei>::max() &&
               height <= std::numeric_limits<GLsizei>::max();
    }

    /// @brief 两个已解析范围只按底层 storage 的 mip/layer 相交判断, 不能比较 GLuint view 名称.
    static bool Overlap(TextureSubresourceSet a, TextureSubresourceSet b) noexcept
    {
        return a.baseMipLevel < b.baseMipLevel + b.numMipLevels && b.baseMipLevel < a.baseMipLevel + a.numMipLevels &&
               a.baseArraySlice < b.baseArraySlice + b.numArraySlices &&
               b.baseArraySlice < a.baseArraySlice + a.numArraySlices;
    }

    const NativeBinding &OpenGLCommandList::BindingFor(const GraphicsState &state, const PlannedResource &entry) const
    {
        // 候选边界已证明布局身份和完整 type/slot 集合, 此处只按当前 Pipeline 的计划定位记录.
        const auto *set = static_cast<const OpenGLBindingSet *>(state.bindings[entry.layoutIndex]);
        const auto found =
            std::ranges::find_if(set->Bindings(), [&](const auto &binding)
                                 { return binding.type == entry.resource.type && binding.slot == entry.slot; });
        assert(found != set->Bindings().end());
        return *found;
    }

    bool OpenGLCommandList::ValidateGraphicsState(const GraphicsState &state, OpenGLGraphicsPipeline *&pipeline,
                                                  OpenGLFramebuffer *&framebuffer)
    {
        const auto fail = [&](const char *reason)
        {
            m_Device->Report("SetGraphicsState", reason);
            return false;
        };
        pipeline = dynamic_cast<OpenGLGraphicsPipeline *>(state.pipeline);
        framebuffer = dynamic_cast<OpenGLFramebuffer *>(state.framebuffer);
        if (!pipeline || !framebuffer || pipeline->Owner() != m_Device.Get() || framebuffer->Owner() != m_Device.Get())
        {
            return fail("InvalidArgument: pipeline/framebuffer implementation or owning device");
        }
        const auto &desc = pipeline->GetDesc();
        // 先确认对象组合和动态输入, 再借用各个不可变描述的预解析 native 计划.
        if (framebuffer->GetFramebufferInfo() != pipeline->GetFramebufferInfo())
        {
            return fail("InterfaceMismatch: framebuffer formats/sampleCount/sampleQuality");
        }
        if (state.shadingRateState.enabled || state.indirectParams || state.indirectCountBuffer ||
            state.viewport.viewports.size() > 1 || state.viewport.scissorRects.size() > 1)
        {
            return fail("Unsupported: indirect, shading-rate or multiple viewport execution");
        }
        for (const auto &viewport : state.viewport.viewports)
        {
            if (!ValidViewport(viewport, m_Device->GetCaps()))
            {
                return fail("InvalidArgument: viewport interval or actual device limits");
            }
        }
        for (const auto &rect : state.viewport.scissorRects)
        {
            if (!ValidScissor(rect))
            {
                return fail("InvalidArgument: scissor interval or native extent");
            }
        }
        if (state.bindings.size() != desc.bindingLayouts.size())
        {
            return fail("InterfaceMismatch: binding set count");
        }
        for (std::size_t index = 0; index < state.bindings.size(); ++index)
        {
            const auto *set = dynamic_cast<OpenGLBindingSet *>(state.bindings[index]);
            if (!set || set->Owner() != m_Device.Get() || set->GetLayout() != desc.bindingLayouts[index].Get())
            {
                return fail("InterfaceMismatch: binding set ownership/layout identity");
            }
        }
        std::array<bool, MaxVertexAttributes> occupied{};
        // VB/IB 的角色, slot 和基础字节偏移在设置边界验证; 本次 Draw 的数据末端另行核对.
        for (const auto &binding : state.vertexBuffers)
        {
            auto *buffer = BufferFor(binding.buffer, "SetGraphicsState");
            if (!buffer || !buffer->GetDesc().isVertexBuffer || binding.slot >= occupied.size() ||
                binding.offset > buffer->GetDesc().byteSize || occupied[binding.slot])
            {
                return fail("InvalidArgument: vertex buffer role, slot, byte offset or duplicate binding");
            }
            occupied[binding.slot] = true;
        }
        if (const auto *input = static_cast<OpenGLInputLayout *>(desc.inputLayout.Get()))
        {
            for (const auto &attribute : input->Inputs())
            {
                if (!occupied[attribute.bufferIndex])
                {
                    return fail("InvalidArgument: required vertex buffer slot missing");
                }
            }
        }
        if (state.indexBuffer.buffer)
        {
            auto *buffer = BufferFor(state.indexBuffer.buffer, "SetGraphicsState");
            const auto format = state.indexBuffer.format;
            if (!buffer || !buffer->GetDesc().isIndexBuffer ||
                (format != Format::R16_UINT && format != Format::R32_UINT) ||
                state.indexBuffer.offset > buffer->GetDesc().byteSize ||
                state.indexBuffer.offset % (format == Format::R16_UINT ? 2 : 4) != 0)
            {
                return fail("InvalidArgument: index buffer role, format or byte offset");
            }
        }
        const auto &depth = desc.renderState.depthStencilState;
        // 深度测试关闭也旁路深度写入; 模板仍有独立的启用位, 写 mask 和双面操作条件.
        if (framebuffer->GetDesc().depthAttachment.isReadOnly &&
            ((depth.depthTestEnable && depth.depthWriteEnable) ||
             (depth.stencilEnable && depth.stencilWriteMask != 0 &&
              (depth.frontFaceStencil.failOp != StencilOp::Keep ||
               depth.frontFaceStencil.depthFailOp != StencilOp::Keep ||
               depth.frontFaceStencil.passOp != StencilOp::Keep || depth.backFaceStencil.failOp != StencilOp::Keep ||
               depth.backFaceStencil.depthFailOp != StencilOp::Keep ||
               depth.backFaceStencil.passOp != StencilOp::Keep))))
        {
            return fail("InvalidArgument: writing pipeline with read-only depth/stencil attachment");
        }
        for (const auto &entry : pipeline->Plan().resources)
        {
            // 活动范围与写附件 feedback 属于本次组合, 不能从独立 Set 创建成功推断.
            const auto &binding = BindingFor(state, entry);
            if (entry.resource.type == ResourceType::ConstantBuffer ||
                entry.resource.type == ResourceType::StructuredBuffer_SRV)
            {
                if (binding.range.byteSize < entry.resource.extent ||
                    (entry.resource.type == ResourceType::StructuredBuffer_SRV &&
                     binding.recordStride != entry.resource.recordStride))
                {
                    return fail("InterfaceMismatch: active buffer block extent/recordStride");
                }
            }
            if (entry.resource.type == ResourceType::Texture_SRV)
            {
                const auto conflicts = [&](const FramebufferAttachment &attachment)
                {
                    return attachment.texture == binding.resource && !attachment.isReadOnly &&
                           Overlap(binding.subresources,
                                   attachment.subresources.Resolve(attachment.texture->GetDesc(), true));
                };
                for (const auto &attachment : framebuffer->GetDesc().colorAttachments)
                {
                    if (conflicts(attachment))
                    {
                        return fail("InvalidState: sampling/writable-attachment feedback");
                    }
                }
                const auto &attachment = framebuffer->GetDesc().depthAttachment;
                if (attachment.texture && conflicts(attachment))
                {
                    return fail("InvalidState: sampling/writable-depth feedback");
                }
            }
        }
        return true;
    }

    void OpenGLCommandList::SetGraphicsState(const GraphicsState &state)
    {
        // 分为纯输入预检, 包引用准备和 native 应用三段; 仅最后一步成功才发布逻辑候选.
        if (!Recording("SetGraphicsState"))
        {
            return;
        }
        OpenGLGraphicsPipeline *pipeline = nullptr;
        OpenGLFramebuffer *framebuffer = nullptr;
        if (!ValidateGraphicsState(state, pipeline, framebuffer))
        {
            return;
        }
        if (!RetainResource(pipeline) || !RetainResource(framebuffer))
        {
            return;
        }
        for (auto *set : state.bindings)
        {
            // 关闭集合自动保活时不偷偷保活其子树, 该集合及依赖到完成前的寿命仍归调用方.
            if (set->GetDesc()->trackLiveness && !RetainResource(set))
            {
                return;
            }
        }
        for (const auto &binding : state.vertexBuffers)
        {
            if (!RetainResource(binding.buffer))
            {
                return;
            }
        }
        if (state.indexBuffer.buffer && !RetainResource(state.indexBuffer.buffer))
        {
            return;
        }
        auto candidate = state;
        // 空动态数组是本次不设置, 保留本 recording 中明确建立过的值, 不借用上一列表的缓存事实.
        if (candidate.viewport.viewports.empty() && m_Packet->viewportSet)
        {
            candidate.viewport.viewports = m_Packet->graphics.viewport.viewports;
        }
        if (candidate.viewport.scissorRects.empty() && m_Packet->scissorSet)
        {
            candidate.viewport.scissorRects = m_Packet->graphics.viewport.scissorRects;
        }
        if (!GraphicsUses(candidate, *pipeline, "SetGraphicsState") || !m_Device->BeginNative("SetGraphicsState"))
        {
            return;
        }
        ApplyGraphics(candidate, *pipeline);
        if (!m_Device->CheckNative("SetGraphicsState"))
        {
            m_Packet->invalid = true;
            m_Device->m_ContextState.Invalidate();
            return;
        }
        m_Packet->graphics = candidate;
        m_Packet->graphicsSet = true;
        m_Packet->viewportSet = !candidate.viewport.viewports.empty();
        m_Packet->scissorSet = !candidate.viewport.scissorRects.empty();
    }
} // namespace Physara::RHI::OpenGL
