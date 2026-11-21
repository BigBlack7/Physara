#include "CommandList.hpp"

/// @file
/// @brief 确定 Draw 的附件和输入用途. Draw 与 Dispatch 共用按 Program 计划应用绑定及选择动态常量的路径.

#include <algorithm>
#include <cassert>

#include "InputLayout.hpp"
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 活动 stage 决定 SRV 读取域, 不因逻辑 set 编号或不活动声明扩大访问.
    static ResourceStates ShaderReads(std::uint32_t stages) noexcept
    {
        auto state = ResourceStates::Unknown;
        if ((stages & 1) != 0)
        {
            state = state | ResourceStates::NonPixelShaderResource;
        }
        if ((stages & 2) != 0)
        {
            state = state | ResourceStates::PixelShaderResource;
        }
        return state;
    }

    // 同格式深度采样同时带上 DepthRead, 供普通采样和只读附件联合采样共用.
    static ResourceStates TextureReads(const OpenGLTexture &texture, ResourceStates shaderReads) noexcept
    {
        return GetFormatInfo(texture.GetDesc().format).hasDepth ? shaderReads | ResourceStates::DepthRead : shaderReads;
    }

    bool OpenGLCommandList::GraphicsUses(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline,
                                         const char *operation, bool executed)
    {
        const auto &depth = state.framebuffer->GetDesc().depthAttachment;
        // Shader 读取按活动计划请求, 然后处理顶点/索引与附件, 不从 native 缓存命中推断用途.
        for (const auto &entry : pipeline.Plan().resources)
        {
            const auto &binding = BindingFor(state.bindings, entry);
            if (binding.type == ResourceType::VolatileConstantBuffer)
            {
                // 版本范围由本包拥有的 host 写建立. 图权限仍用公开 Buffer 的逻辑范围.
                if (!CheckGraphBufferUse(*static_cast<OpenGLBuffer *>(binding.resource), ResourceStates::ConstantBuffer,
                                         operation, binding.range))
                {
                    return false;
                }
            }
            else if (binding.type == ResourceType::ConstantBuffer || binding.type == ResourceType::StructuredBuffer_SRV)
            {
                const auto use = binding.type == ResourceType::ConstantBuffer ? ResourceStates::ConstantBuffer
                                                                              : ShaderReads(entry.stages);
                if (!UseBuffer(*static_cast<OpenGLBuffer *>(binding.resource), use, operation, binding.range,
                               binding.type == ResourceType::StructuredBuffer_SRV, StorageAccess::None,
                               executed ? (binding.type == ResourceType::ConstantBuffer ? GL_UNIFORM_BARRIER_BIT :
                                                                                         GL_SHADER_STORAGE_BARRIER_BIT) : 0))
                {
                    return false;
                }
            }
            else if (binding.type == ResourceType::Texture_SRV)
            {
                auto &texture = *static_cast<OpenGLTexture *>(binding.resource);
                const auto shaderReads = ShaderReads(entry.stages);
                const auto usesDepth = GetFormatInfo(texture.GetDesc().format).hasDepth;
                const auto aspect = usesDepth ? TextureAspect::Depth : TextureAspect::Color;
                if (!UseTexture(texture, binding.subresources, aspect, TextureReads(texture, shaderReads),
                                operation, shaderReads, StorageAccess::None, executed ? GL_TEXTURE_FETCH_BARRIER_BIT : 0))
                {
                    return false;
                }
            }
        }
        if (const auto *input = static_cast<OpenGLInputLayout *>(pipeline.GetDesc().inputLayout.Get()))
        {
            std::array<bool, MaxVertexAttributes> used{};
            for (const auto &attribute : input->Inputs())
            {
                if (used[attribute.bufferIndex])
                {
                    continue;
                }
                used[attribute.bufferIndex] = true;
                const auto found =
                    std::ranges::find(state.vertexBuffers, attribute.bufferIndex, &VertexBufferBinding::slot);
                // 绑定时先确认 VB 角色. 实际元素区间由 Draw 在取得命令参数之后检查.
                if (!UseBuffer(*static_cast<OpenGLBuffer *>(found->buffer), ResourceStates::VertexBuffer, operation))
                {
                    return false;
                }
            }
        }
        if (state.indexBuffer.buffer &&
            !UseBuffer(*static_cast<OpenGLBuffer *>(state.indexBuffer.buffer), ResourceStates::IndexBuffer, operation))
        {
            return false;
        }
        for (const auto &attachment : state.framebuffer->GetDesc().colorAttachments)
        {
            if (!UseTexture(*static_cast<OpenGLTexture *>(attachment.texture), attachment.subresources,
                            TextureAspect::Color, ResourceStates::RenderTarget, operation, ResourceStates::Unknown,
                            StorageAccess::None, executed ? GL_FRAMEBUFFER_BARRIER_BIT : 0))
            {
                return false;
            }
        }
        if (depth.texture && depth.isReadOnly)
        {
            // 合并与深度附件相交的活动采样阶段. Automatic 沿该组合更新 tracker, GraphManaged 核对已提交的值.
            auto use = ResourceStates::DepthRead;
            for (const auto &entry : pipeline.Plan().resources)
            {
                const auto &binding = BindingFor(state.bindings, entry);
                if (SamplesAttachment(binding, depth))
                {
                    use = use | ShaderReads(entry.stages);
                }
            }
            return UseTexture(*static_cast<OpenGLTexture *>(depth.texture), depth.subresources, depth.aspects,
                              use, operation, ResourceStates::DepthRead);
        }
        return !depth.texture ||
               UseTexture(*static_cast<OpenGLTexture *>(depth.texture), depth.subresources, depth.aspects,
                          ResourceStates::DepthWrite, operation);
    }

    void OpenGLCommandList::ApplyGraphics(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline)
    {
        // 先建立目标/光栅与输入, 再消费本 Pipeline 的物理映射; 所有 compare-then-set 归 ContextState.
        auto &context = m_Device->m_ContextState;
        context.ApplyProgram(pipeline.Name(),
                             static_cast<OpenGLShader *>(pipeline.GetDesc().VS.Get())->WritesPointSize());
        context.ApplyFramebuffer(static_cast<OpenGLFramebuffer *>(state.framebuffer)->Name());
        context.ApplyRenderState(pipeline.GetDesc().renderState, pipeline.GetFramebufferInfo(),
                                 state.blendConstantColor, state.dynamicStencilRefValue);
        if (!state.viewport.viewports.empty())
        {
            context.ApplyViewport(state.viewport.viewports[0]);
        }
        if (!state.viewport.scissorRects.empty())
        {
            context.ApplyScissor(state.viewport.scissorRects[0]);
        }
        const auto *input = static_cast<OpenGLInputLayout *>(pipeline.GetDesc().inputLayout.Get());
        context.ApplyInput(input);
        if (input)
        {
            std::array<bool, MaxVertexAttributes> used{};
            for (const auto &attribute : input->Inputs())
            {
                if (used[attribute.bufferIndex])
                {
                    continue;
                }
                used[attribute.bufferIndex] = true;
                const auto found =
                    std::ranges::find(state.vertexBuffers, attribute.bufferIndex, &VertexBufferBinding::slot);
                context.ApplyVertexBuffer(attribute.bufferIndex, static_cast<OpenGLBuffer *>(found->buffer)->Name(),
                                          found->offset, GLsizei(attribute.stride), attribute.instanced ? 1 : 0);
            }
        }
        context.ApplyIndexBuffer(
            state.indexBuffer.buffer ? static_cast<OpenGLBuffer *>(state.indexBuffer.buffer)->Name() : 0);
        ApplyProgramBindings(state.bindings, pipeline.Plan());
    }

    void OpenGLCommandList::ApplyProgramBindings(const BindingSetVector &bindings, const PipelineBindingPlan &plan)
    {
        auto &context = m_Device->m_ContextState;
        for (const auto &entry : plan.resources)
        {
            const auto &binding = BindingFor(bindings, entry);
            if (binding.type == ResourceType::ConstantBuffer || binding.type == ResourceType::StructuredBuffer_SRV ||
                binding.type == ResourceType::StructuredBuffer_UAV)
            {
                context.ApplyBufferRange(binding.type != ResourceType::ConstantBuffer, entry.unit, binding.name,
                                         GLintptr(binding.range.byteOffset), GLsizeiptr(binding.range.byteSize));
            }
            else if (binding.type == ResourceType::Texture_UAV)
            {
                const auto access = entry.resource.access;
                const GLenum native = access == StorageAccess::Read ? GL_READ_ONLY :
                                      (access == StorageAccess::Write ? GL_WRITE_ONLY : GL_READ_WRITE);
                context.ApplyImage(entry.unit, binding.name, binding.dimension == TextureDimension::Texture2DArray,
                                   native, binding.imageInternalFormat);
            }
        }
        // pair 的两端可来自不同集合, 不能把 texture 的逻辑 slot 当作 sampler 或原生单元.
        const auto find = [&](ShaderBindingKey key) -> const NativeBinding &
        {
            const auto entry = std::ranges::find(plan.resources, key,
                                                 [](const auto &resource) { return resource.resource.key; });
            assert(entry != plan.resources.end());
            return BindingFor(bindings, *entry);
        };
        for (const auto &pair : plan.pairs)
        {
            const auto &texture = find(pair.pair.texture);
            const auto sampler = pair.pair.dummy ? m_Device->m_DummySampler->name : find(pair.pair.sampler).name;
            context.ApplyTexture(pair.unit, texture.name, sampler, texture.dimension);
        }
    }

    void OpenGLCommandList::SetResourceStatesForBindingSet(BindingSet *bindingSet)
    {
        // 此入口只形成待提交的用途请求; 单独保活实际资源以保护 tracker 对 extension 的借用.
        if (!StateRecording("SetResourceStatesForBindingSet"))
        {
            return;
        }
        auto *set = dynamic_cast<OpenGLBindingSet *>(bindingSet);
        if (!set || set->Owner() != m_Device.Get())
        {
            m_Device->Report("SetResourceStatesForBindingSet", "InvalidArgument: binding set ownership");
            return;
        }
        const auto shaderState = GetShaderResourceStateForBindingLayout(set->GetLayout());
        for (const auto &binding : set->Bindings())
        {
            if (binding.type == ResourceType::Sampler || binding.type == ResourceType::PushConstants ||
                binding.type == ResourceType::VolatileConstantBuffer)
            {
                continue;
            }
            if (!RetainResource(binding.resource))
            {
                return;
            }
            if (binding.type == ResourceType::Texture_SRV || binding.type == ResourceType::Texture_UAV)
            {
                auto *texture = static_cast<OpenGLTexture *>(binding.resource);
                const auto use = binding.type == ResourceType::Texture_UAV ? ResourceStates::UnorderedAccess :
                                                                             TextureReads(*texture, shaderState);
                m_Packet->tracker.RequireTextureState(&texture->m_State, binding.subresources, use);
            }
            else
            {
                auto *buffer = static_cast<OpenGLBuffer *>(binding.resource);
                const auto use = binding.type == ResourceType::ConstantBuffer ? ResourceStates::ConstantBuffer :
                    (binding.type == ResourceType::StructuredBuffer_UAV ? ResourceStates::UnorderedAccess : shaderState);
                m_Packet->tracker.RequireBufferState(&buffer->m_State, use);
            }
        }
    }

    bool OpenGLCommandList::CheckConstants(const BindingSetVector &bindings, const PipelineBindingPlan &plan,
                                               const char *operation) const
    {
        for (const auto &entry : plan.resources)
        {
            if (entry.bindingType == ResourceType::PushConstants && !m_Packet->push.buffer)
            {
                m_Device->Report(operation, "InvalidState: push constants missing for current pipeline");
                return false;
            }
            if (entry.bindingType == ResourceType::VolatileConstantBuffer)
            {
                const auto &binding = BindingFor(bindings, entry);
                const auto &buffer = *static_cast<OpenGLBuffer *>(binding.resource);
                // 成员有效字节须落在本次写入前缀内. 原生绑定另计块尾对齐, 整段须落在同一版本槽内.
                if (buffer.m_LatestRecording != m_Packet->recordingId || buffer.m_ValidPrefix < entry.resource.extent ||
                    entry.nativeExtent > buffer.m_VersionStride)
                {
                    m_Device->Report(operation, "InvalidState: volatile version missing or too small for active block",
                                     buffer.GetDesc().debugName.c_str());
                    return false;
                }
            }
        }
        return true;
    }

    void OpenGLCommandList::ApplyConstants(const BindingSetVector &bindings, const PipelineBindingPlan &plan)
    {
        auto &context = m_Device->m_ContextState;
        for (const auto &entry : plan.resources)
        {
            if (entry.bindingType == ResourceType::PushConstants)
            {
                const auto &push = m_Packet->push;
                context.ApplyBufferRange(false, entry.unit, push.buffer, GLintptr(push.offset), entry.nativeExtent);
            }
            else if (entry.bindingType == ResourceType::VolatileConstantBuffer)
            {
                const auto &buffer = *static_cast<OpenGLBuffer *>(BindingFor(bindings, entry).resource);
                const auto offset = buffer.m_LatestVersion * buffer.m_VersionStride;
                context.ApplyBufferRange(false, entry.unit, buffer.Name(), GLintptr(offset), entry.nativeExtent);
            }
        }
    }
} // namespace Physara::RHI::OpenGL
