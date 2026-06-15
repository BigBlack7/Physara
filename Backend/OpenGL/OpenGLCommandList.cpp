#include "OpenGLCommandList.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/Core/Log.hpp>

#include <Backend/OpenGL/OpenGLBuffer.hpp>
#include <Backend/OpenGL/OpenGLFramebuffer.hpp>
#include <Backend/OpenGL/OpenGLPipeline.hpp>
#include <Backend/OpenGL/OpenGLSampler.hpp>
#include <Backend/OpenGL/OpenGLTexture.hpp>
#include <Backend/OpenGL/OpenGLTypeMapping.hpp>

namespace Physara::RHI
{
    namespace OpenGLCommandListDetail
    {
        static bool BlendStateEqual(const RHIBlendState &a, const RHIBlendState &b)
        {
            return a.blendEnable == b.blendEnable &&
                   a.srcColor == b.srcColor &&
                   a.dstColor == b.dstColor &&
                   a.colorOp == b.colorOp &&
                   a.srcAlpha == b.srcAlpha &&
                   a.dstAlpha == b.dstAlpha &&
                   a.alphaOp == b.alphaOp;
        }

        static bool IsDepthStencilFormat(TextureFormat fmt)
        {
            return fmt == TextureFormat::Depth24Stencil8;
        }

        // OpenGL的索引类型只能是GL_UNSIGNED_SHORT或GL_UNSIGNED_INT, 这里根据当前绑定的indexType计算每个索引的字节大小
        static std::uint32_t GetIndexStride(GLenum indexType)
        {
            if (indexType == GL_UNSIGNED_SHORT)
            {
                return 2u;
            }
            return 4u;
        }

        static GLbitfield ToGLMemoryBarrierBits(const RHIResourceBarrier &barrier)
        {
            // OpenGL没有Vulkan那种显式layout/state transition; 这里把RHI的资源状态语义
            // 翻译成glMemoryBarrier的可见性bit, 保证前序shader/FBO/copy写入对后续读写可见
            const bool framebufferWriteToShaderRead =
                (barrier.before == ResourceState::RenderTarget || barrier.before == ResourceState::DepthWrite) &&
                barrier.after == ResourceState::ShaderResource &&
                (barrier.srcAccess & (ResourceAccess::ColorAttachmentWrite | ResourceAccess::DepthStencilWrite)) != 0u &&
                (barrier.dstAccess & ResourceAccess::ShaderRead) != 0u &&
                (barrier.srcAccess & (ResourceAccess::ShaderWrite | ResourceAccess::TransferWrite)) == 0u &&
                (barrier.dstAccess & (ResourceAccess::ShaderWrite | ResourceAccess::TransferWrite)) == 0u;
            if (framebufferWriteToShaderRead)
            {
                return 0;
            }

            GLbitfield bits = 0;

            auto hasBeforeOrAfter = [&barrier](ResourceState state)
            {
                return barrier.before == state || barrier.after == state;
            };

            if ((barrier.dstAccess & ResourceAccess::ShaderRead) != 0u ||
                (barrier.srcAccess & ResourceAccess::ShaderWrite) != 0u ||
                hasBeforeOrAfter(ResourceState::ShaderResource))
            {
                // 纹理采样、image load/store、SSBO读写都属于shader可见性
                bits |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::ShaderWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::ShaderWrite) != 0u ||
                hasBeforeOrAfter(ResourceState::UnorderedAccess))
            {
                // image store和SSBO写入后续shader可见性, 包括同样的shader write和后续的shader read/write
                bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::VertexAttributeRead) != 0u ||
                hasBeforeOrAfter(ResourceState::VertexBuffer))
            {
                // 顶点输入的可见性, 包括顶点属性和实例属性
                bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::IndexRead) != 0u ||
                hasBeforeOrAfter(ResourceState::IndexBuffer))
            {
                // 索引缓冲的可见性
                bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::IndirectCommandRead) != 0u ||
                hasBeforeOrAfter(ResourceState::IndirectArgument))
            {
                // compute/SSBO/image写入间接绘制命令后, 后续MDI读取需要command barrier可见性
                bits |= GL_COMMAND_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::UniformRead) != 0u ||
                hasBeforeOrAfter(ResourceState::ConstantBuffer))
            {
                // Uniform缓冲的可见性
                bits |= GL_UNIFORM_BARRIER_BIT;
            }

            if ((barrier.srcAccess & (ResourceAccess::TransferWrite | ResourceAccess::ColorAttachmentWrite)) != 0u ||
                (barrier.dstAccess & (ResourceAccess::TransferRead | ResourceAccess::TransferWrite)) != 0u ||
                hasBeforeOrAfter(ResourceState::CopySource) ||
                hasBeforeOrAfter(ResourceState::CopyDest))
            {
                // 复制和渲染目标的可见性, 包括纹理更新、缓冲更新、以及对copy source/dest的读写
                bits |= GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::ColorAttachmentWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::ColorAttachmentRead) != 0u ||
                hasBeforeOrAfter(ResourceState::RenderTarget))
            {
                // 渲染目标的可见性, 包括对当前绑定FBO的读写以及后续shader对渲染结果的采样/读写
                bits |= GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::DepthStencilWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::DepthStencilRead) != 0u ||
                hasBeforeOrAfter(ResourceState::DepthWrite) ||
                hasBeforeOrAfter(ResourceState::DepthRead))
            {
                // 深度模板的可见性, 包括对当前绑定FBO的深度写入以及后续shader对深度纹理的采样/读写
                bits |= GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::HostWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::HostRead) != 0u)
            {
                // CPU写入后GPU可见
                bits |= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
            }

            // OpenGL没有Vulkan那种显式layout/state transition, 如果RHI层没有明确指定访问类型和资源状态, 就当作全能barrier
            return bits != 0 ? bits : GL_ALL_BARRIER_BITS;
        }
    }

    OpenGLCommandList::OpenGLCommandList()
    {
        InvalidateBindingCache();

        // 用一个小UBO模拟push constants. OpenGL没有原生push constant;
        // 这里固定绑定到slot 0, 后续shader约定一个小uniform block即可读取
        glCreateBuffers(1, &m_PushConstantsBuffer);
        if (m_PushConstantsBuffer == 0)
        {
            PHYSARA_CORE_ERROR("Failed to create push constants buffer.");
            return;
        }

        glNamedBufferStorage(
            m_PushConstantsBuffer,
            static_cast<GLsizeiptr>(kPushConstantsSize),
            nullptr,
            GL_DYNAMIC_STORAGE_BIT);

        glCreateFramebuffers(1, &m_ResolveReadFramebuffer);
        glCreateFramebuffers(1, &m_ResolveDrawFramebuffer);
    }

    void OpenGLCommandList::InvalidateBindingCache()
    {
        for (BufferRangeBindingState &binding : m_UniformBufferBindings)
        {
            binding.valid = false;
        }
        for (BufferRangeBindingState &binding : m_StorageBufferBindings)
        {
            binding.valid = false;
        }
        m_TextureBindings.fill(std::numeric_limits<GLuint>::max());
        m_SamplerBindings.fill(std::numeric_limits<GLuint>::max());
        for (ImageTextureBindingState &binding : m_ImageTextureBindings)
        {
            binding.valid = false;
        }
        m_IndirectBufferBinding.valid = false;
    }

    void OpenGLCommandList::InvalidatePipelineState()
    {
        m_PipelineStateValid = false;
        m_State.program = 0;
        m_State.vao = 0;
        for (ColorMaskState &state : m_ColorMaskStates)
        {
            state.valid = false;
        }
        m_DepthMaskState.valid = false;
        m_StencilMaskState.valid = false;
        InvalidateVertexInputCache();
    }

    void OpenGLCommandList::InvalidateVertexInputCache()
    {
        for (VertexBufferBindingState &binding : m_VertexBufferBindings)
        {
            binding.valid = false;
        }
        m_IndexBufferBinding.valid = false;
        m_RenderPrimitiveBinding.valid = false;
    }

    void OpenGLCommandList::InvalidateDynamicStateCache()
    {
        m_ViewportState.valid = false;
        m_ScissorState.valid = false;
    }

    void OpenGLCommandList::InvalidateRenderPassStateCache()
    {
        m_FramebufferBinding.valid = false;
        m_DefaultDrawBuffers.valid = false;
    }

    void OpenGLCommandList::BindFramebuffer(GLuint framebuffer)
    {
        if (m_FramebufferBinding.valid && m_FramebufferBinding.framebuffer == framebuffer)
        {
            return;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        ++m_Statistics.framebufferBinds;
        m_FramebufferBinding = FramebufferBindingState{framebuffer, true};
    }

    void OpenGLCommandList::ConfigureDefaultDrawBuffers(std::uint32_t colorCount)
    {
        if (colorCount == 0u)
        {
            m_DefaultDrawBuffers.count = 0u;
            m_DefaultDrawBuffers.valid = true;
            return;
        }

        colorCount = std::min(colorCount, kMaxColorAttachments);
        std::array<GLenum, kMaxColorAttachments> drawBuffers{};
        for (std::uint32_t i = 0; i < colorCount; ++i)
        {
            drawBuffers[i] = GL_BACK;
        }

        bool dirty = !m_DefaultDrawBuffers.valid || m_DefaultDrawBuffers.count != colorCount;
        for (std::uint32_t i = 0; i < colorCount && !dirty; ++i)
        {
            dirty = m_DefaultDrawBuffers.buffers[i] != drawBuffers[i];
        }
        if (!dirty)
        {
            return;
        }

        glDrawBuffers(static_cast<GLsizei>(colorCount), colorCount > 0u ? drawBuffers.data() : nullptr);
        m_DefaultDrawBuffers.buffers = drawBuffers;
        m_DefaultDrawBuffers.count = colorCount;
        m_DefaultDrawBuffers.valid = true;
    }

    void OpenGLCommandList::SetColorMask(std::uint32_t attachment, bool red, bool green, bool blue, bool alpha)
    {
        if (attachment >= m_ColorMaskStates.size())
        {
            glColorMaski(
                attachment,
                red ? GL_TRUE : GL_FALSE,
                green ? GL_TRUE : GL_FALSE,
                blue ? GL_TRUE : GL_FALSE,
                alpha ? GL_TRUE : GL_FALSE);
            return;
        }

        ColorMaskState &state = m_ColorMaskStates[attachment];
        if (state.valid &&
            state.red == red &&
            state.green == green &&
            state.blue == blue &&
            state.alpha == alpha)
        {
            return;
        }

        glColorMaski(
            attachment,
            red ? GL_TRUE : GL_FALSE,
            green ? GL_TRUE : GL_FALSE,
            blue ? GL_TRUE : GL_FALSE,
            alpha ? GL_TRUE : GL_FALSE);
        state = ColorMaskState{red, green, blue, alpha, true};
    }

    void OpenGLCommandList::SetDepthMaskState(bool enabled)
    {
        if (m_DepthMaskState.valid && m_DepthMaskState.enabled == enabled)
        {
            return;
        }

        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
        m_DepthMaskState = DepthMaskState{enabled, true};
        m_State.depthWrite = enabled;
    }

    void OpenGLCommandList::SetStencilMaskState(GLuint mask)
    {
        if (m_StencilMaskState.valid && m_StencilMaskState.mask == mask)
        {
            return;
        }

        glStencilMask(mask);
        m_StencilMaskState = StencilMaskState{mask, true};
    }

    void OpenGLCommandList::SetScissorEnabled(bool enabled)
    {
        if (m_ScissorState.valid && m_ScissorState.enabled == enabled)
        {
            return;
        }

        if (enabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
        m_ScissorState.enabled = enabled;
        m_ScissorState.valid = true;
    }

    OpenGLCommandList::~OpenGLCommandList()
    {
        if (m_ResolveReadFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_ResolveReadFramebuffer);
            m_ResolveReadFramebuffer = 0;
        }
        if (m_ResolveDrawFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_ResolveDrawFramebuffer);
            m_ResolveDrawFramebuffer = 0;
        }
        if (m_PushConstantsBuffer != 0)
        {
            glDeleteBuffers(1, &m_PushConstantsBuffer);
            m_PushConstantsBuffer = 0;
        }
    }

    void OpenGLCommandList::ResetStatistics()
    {
        m_Statistics.Reset();
    }

    RHICommandStatistics OpenGLCommandList::GetStatistics() const
    {
        return m_Statistics;
    }

    void OpenGLCommandList::SetPipelineState(RHIPipelineState *pso)
    {
        // PipelineState在OpenGL下拆成program + VAO + 固定函数状态
        // ImGui等外部OpenGL代码会改写全局状态, 因此这里显式重绑关键状态, 避免缓存与真实GL状态脱节。
        auto *gl = static_cast<OpenGLPipeline *>(pso);
        if (gl == nullptr || !gl->IsValid())
        {
            PHYSARA_CORE_ERROR("Invalid pipeline state.");
            return;
        }

        const bool invalidState = !m_PipelineStateValid;
        const GLuint program = gl->GetProgram();
        if (invalidState || m_State.program != program)
        {
            glUseProgram(program);
            ++m_Statistics.programBinds;
            m_State.program = program;
        }

        if (!gl->IsCompute())
        {
            const GLuint vao = gl->GetVAO();
            if (invalidState || m_State.vao != vao)
            {
                glBindVertexArray(vao);
                ++m_Statistics.vaoBinds;
                m_State.vao = vao;
                InvalidateVertexInputCache();
            }
        }

        const auto &desc = gl->GetDesc();
        m_CurrentPipelineDesc = &desc;

        if (gl->IsCompute())
        {
            // Compute pipeline只需要program, 后面的raster/depth/blend状态对compute无意义
            m_PipelineStateValid = true;
            return;
        }

        if (invalidState)
        {
            glDisable(GL_RASTERIZER_DISCARD);
            glDisable(GL_STENCIL_TEST);
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            glDisable(GL_SAMPLE_COVERAGE);
        }

        const bool currentCullEnabled = m_State.cullMode != CullMode::None;
        const bool targetCullEnabled = desc.rasterizerState.cullMode != CullMode::None;
        if (invalidState || currentCullEnabled != targetCullEnabled)
        {
            if (targetCullEnabled)
            {
                glEnable(GL_CULL_FACE);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
        }
        if (targetCullEnabled && (invalidState || m_State.cullMode != desc.rasterizerState.cullMode))
        {
            glCullFace(ToGLCullMode(desc.rasterizerState.cullMode));
        }
        if (invalidState || m_State.cullMode != desc.rasterizerState.cullMode)
        {
            m_State.cullMode = desc.rasterizerState.cullMode;
        }

        if (invalidState || m_State.polygonMode != desc.rasterizerState.polygonMode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, ToGLPolygonMode(desc.rasterizerState.polygonMode));
            m_State.polygonMode = desc.rasterizerState.polygonMode;
        }

        const bool currentDepthBiasEnabled = m_State.depthBias != 0.f || m_State.depthBiasSlope != 0.f;
        const bool targetDepthBiasEnabled = desc.rasterizerState.depthBias != 0.f || desc.rasterizerState.depthBiasSlope != 0.f;
        if (invalidState || currentDepthBiasEnabled != targetDepthBiasEnabled)
        {
            if (targetDepthBiasEnabled)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
            }
            else
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
        }
        if (targetDepthBiasEnabled &&
            (invalidState ||
             m_State.depthBias != desc.rasterizerState.depthBias ||
             m_State.depthBiasSlope != desc.rasterizerState.depthBiasSlope))
        {
            glPolygonOffset(desc.rasterizerState.depthBiasSlope, desc.rasterizerState.depthBias);
        }
        m_State.depthBias = desc.rasterizerState.depthBias;
        m_State.depthBiasSlope = desc.rasterizerState.depthBiasSlope;

        if (invalidState || m_State.depthTest != desc.depthStencilState.depthTest)
        {
            if (desc.depthStencilState.depthTest)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
            m_State.depthTest = desc.depthStencilState.depthTest;
        }

        if (invalidState || m_State.depthWrite != desc.depthStencilState.depthWrite)
        {
            SetDepthMaskState(desc.depthStencilState.depthWrite);
        }

        if (invalidState || m_State.depthFunc != desc.depthStencilState.compareOp)
        {
            glDepthFunc(ToGLDepthFunc(desc.depthStencilState.compareOp));
            m_State.depthFunc = desc.depthStencilState.compareOp;
        }

        for (std::uint32_t i = 0; i < kMaxColorAttachments; ++i)
        {
            RHIBlendState target{};
            if (i < desc.blendStates.size())
            {
                target = desc.blendStates[i];
            }

            const bool blendDirty = invalidState || !OpenGLCommandListDetail::BlendStateEqual(m_State.blendStates[i], target);
            if (blendDirty)
            {
                if (invalidState || m_State.blendStates[i].blendEnable != target.blendEnable)
                {
                    if (target.blendEnable)
                    {
                        glEnablei(GL_BLEND, i);
                    }
                    else
                    {
                        glDisablei(GL_BLEND, i);
                    }
                }
                if (target.blendEnable)
                {
                    // OpenGL的多重blend state是基于draw buffer index的, 这里假设color attachment 0对应draw buffer 0。
                    glBlendFuncSeparatei(
                        i,
                        ToGLBlendFactor(target.srcColor),
                        ToGLBlendFactor(target.dstColor),
                        ToGLBlendFactor(target.srcAlpha),
                        ToGLBlendFactor(target.dstAlpha));
                    glBlendEquationSeparatei(
                        i,
                        ToGLBlendOp(target.colorOp),
                        ToGLBlendOp(target.alphaOp));
                }
                m_State.blendStates[i] = target;
            }

            if (invalidState)
            {
                SetColorMask(i, true, true, true, true);
            }
        }

        m_State.topology = ToGLTopology(desc.topology);
        m_PipelineStateValid = true;
    }

    void OpenGLCommandList::SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset)
    {
        // DSA 路径: 直接把buffer绑定到VAO的vertex binding slot,
        // 不需要先glBindVertexArray + glBindBuffer(GL_ARRAY_BUFFER)
        if (m_State.vao == 0)
        {
            PHYSARA_CORE_ERROR("SetVertexBuffer called without a bound VAO.");
            return;
        }
        m_RenderPrimitiveBinding.valid = false;

        GLuint id = 0;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            id = glBuffer->GetGLID();
        }

        std::uint32_t stride = 0;
        if (m_CurrentPipelineDesc)
        {
            for (const auto &b : m_CurrentPipelineDesc->vertexBindings)
            {
                if (b.binding == binding)
                {
                    stride = b.stride;
                    break;
                }
            }
        }

        if (binding >= m_VertexBufferBindings.size())
        {
            glVertexArrayVertexBuffer(
                m_State.vao,
                binding,
                id,
                static_cast<GLintptr>(offset),
                static_cast<GLsizei>(stride));
            ++m_Statistics.vertexBufferBinds;
            return;
        }

        VertexBufferBindingState &cached = m_VertexBufferBindings[binding];
        if (cached.valid && cached.buffer == id && cached.offset == offset && cached.stride == stride)
        {
            return;
        }

        glVertexArrayVertexBuffer(
            m_State.vao,
            binding,
            id,
            static_cast<GLintptr>(offset),
            static_cast<GLsizei>(stride));
        ++m_Statistics.vertexBufferBinds;
        cached = VertexBufferBindingState{id, offset, stride, true};
    }

    void OpenGLCommandList::SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset)
    {
        // DSA路径: Element buffer是VAO状态的一部分, 直接写入当前 VAO
        // indexOffset由DrawIndexed转成byte offset
        if (m_State.vao == 0)
        {
            PHYSARA_CORE_ERROR("SetIndexBuffer called without a bound VAO.");
            return;
        }
        m_RenderPrimitiveBinding.valid = false;

        GLuint id = 0;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            id = glBuffer->GetGLID();
        }

        if (!m_IndexBufferBinding.valid || m_IndexBufferBinding.buffer != id)
        {
            glVertexArrayElementBuffer(m_State.vao, id);
            ++m_Statistics.indexBufferBinds;
            m_IndexBufferBinding.buffer = id;
            m_IndexBufferBinding.valid = true;
        }
        m_State.indexOffset = offset;
        m_State.indexType = GL_UNSIGNED_INT;
        m_IndexBufferBinding.offset = offset;
        m_IndexBufferBinding.indexType = m_State.indexType;
    }

    void OpenGLCommandList::SetRenderPrimitive(const RHIRenderPrimitive &primitive)
    {
        if (primitive.stableId != 0 && m_RenderPrimitiveBinding.valid &&
            m_RenderPrimitiveBinding.stableId == primitive.stableId)
        {
            return;
        }

        ++m_Statistics.renderPrimitiveBinds;
        for (const RHIVertexBufferBinding &binding : primitive.vertexBuffers)
        {
            SetVertexBuffer(binding.slot, binding.buffer, binding.offset);
        }
        SetIndexBuffer(primitive.indexBuffer.buffer, primitive.indexBuffer.offset);
        m_RenderPrimitiveBinding = RenderPrimitiveBindingState{primitive.stableId, primitive.stableId != 0};
    }

    void OpenGLCommandList::SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer)
    {
        const std::uint32_t size = buffer != nullptr ? buffer->GetSize() : 0u;
        SetUniformBuffer(slot, buffer, 0u, size);
    }

    void OpenGLCommandList::SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer, std::uint32_t offset, std::uint32_t size)
    {
        // UBO仍然是binding point语义; range overload为后续frame upload allocator提供子分配绑定入口.
        BindBufferRange(GL_UNIFORM_BUFFER, false, slot, buffer, offset, size);
    }

    void OpenGLCommandList::SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer)
    {
        const std::uint32_t size = buffer != nullptr ? buffer->GetSize() : 0u;
        SetStorageBuffer(slot, buffer, 0u, size);
    }

    void OpenGLCommandList::SetTexture(std::uint32_t slot, RHITexture *texture, RHISampler *sampler)
    {
        // DSA风格的texture unit绑定. Texture和Sampler分离, 便于复用同一纹理配不同采样器
        GLuint texID = 0;
        GLuint samplerID = 0;

        if (texture)
        {
            auto *glTex = static_cast<OpenGLTexture *>(texture);
            texID = glTex->GetGLID();
        }

        if (sampler)
        {
            auto *glSampler = static_cast<OpenGLSampler *>(sampler);
            samplerID = glSampler->GetGLID();
        }

        if (slot >= m_TextureBindings.size() || m_TextureBindings[slot] != texID)
        {
            glBindTextureUnit(slot, texID);
            ++m_Statistics.textureBinds;
            if (slot < m_TextureBindings.size())
            {
                m_TextureBindings[slot] = texID;
            }
        }
        if (slot >= m_SamplerBindings.size() || m_SamplerBindings[slot] != samplerID)
        {
            glBindSampler(slot, samplerID);
            ++m_Statistics.samplerBinds;
            if (slot < m_SamplerBindings.size())
            {
                m_SamplerBindings[slot] = samplerID;
            }
        }
    }

    void OpenGLCommandList::SetResourceSet(std::uint32_t setIndex, const RHIResourceSet &resourceSet)
    {
        (void)setIndex;
        if (resourceSet.textures.empty())
        {
            return;
        }

        if (BindTextureRange(resourceSet.textures))
        {
            ++m_Statistics.resourceSetBinds;
        }
    }

    void OpenGLCommandList::SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer, std::uint32_t offset, std::uint32_t size)
    {
        // SSBO用于大块结构化数据, 例如object data、light list、tile light indices
        BindBufferRange(GL_SHADER_STORAGE_BUFFER, true, slot, buffer, offset, size);
    }

    void OpenGLCommandList::BindRawBufferRange(
        GLenum target,
        std::array<BufferRangeBindingState, 32> &bindings,
        std::uint64_t &bindCounter,
        std::uint32_t slot,
        GLuint buffer,
        std::uint32_t offset,
        std::uint32_t size)
    {
        if (slot >= bindings.size())
        {
            if (buffer != 0 && size != 0u)
            {
                glBindBufferRange(target, slot, buffer, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
            }
            else
            {
                glBindBufferBase(target, slot, buffer);
            }
            ++bindCounter;
            return;
        }

        BufferRangeBindingState &cached = bindings[slot];
        if (cached.valid && cached.buffer == buffer && cached.offset == offset && cached.size == size)
        {
            return;
        }

        if (buffer != 0 && size != 0u)
        {
            glBindBufferRange(target, slot, buffer, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
        }
        else
        {
            glBindBufferBase(target, slot, buffer);
        }
        ++bindCounter;
        cached = BufferRangeBindingState{buffer, offset, size, true};
    }

    void OpenGLCommandList::BindBufferRange(
        GLenum target,
        bool storageBuffer,
        std::uint32_t slot,
        RHIBuffer *buffer,
        std::uint32_t offset,
        std::uint32_t size)
    {
        GLuint id = 0;
        std::uint32_t bindOffset = 0u;
        std::uint32_t bindSize = 0u;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            if (size == 0u || offset > glBuffer->GetSize() || size > glBuffer->GetSize() - offset)
            {
                PHYSARA_CORE_ERROR("Set buffer range out of bounds: slot={}, offset={}, size={}, bufferSize={}.",
                                   slot,
                                   offset,
                                   size,
                                   glBuffer->GetSize());
                return;
            }

            id = glBuffer->GetGLID();
            bindOffset = glBuffer->GetBindOffset() + offset;
            bindSize = size;
        }

        auto &bindings = storageBuffer ? m_StorageBufferBindings : m_UniformBufferBindings;
        std::uint64_t &bindCounter = storageBuffer ? m_Statistics.storageBufferBinds : m_Statistics.uniformBufferBinds;
        BindRawBufferRange(target, bindings, bindCounter, slot, id, bindOffset, bindSize);
    }

    bool OpenGLCommandList::BindTextureRange(std::span<const RHITextureBinding> bindings)
    {
        bool submitted = false;
        std::uint32_t begin = 0;
        while (begin < bindings.size())
        {
            const std::uint32_t firstSlot = bindings[begin].slot;
            std::array<GLuint, 32> textureIds{};
            std::array<GLuint, 32> samplerIds{};
            std::array<std::uint32_t, 32> slots{};
            std::array<bool, 32> textureDirtySlots{};
            std::array<bool, 32> samplerDirtySlots{};
            std::uint32_t count = 0;

            for (std::uint32_t i = begin; i < bindings.size(); ++i)
            {
                const RHITextureBinding &binding = bindings[i];
                if (binding.slot != firstSlot + count || count >= textureIds.size())
                {
                    break;
                }

                GLuint textureId = 0;
                GLuint samplerId = 0;
                if (binding.texture != nullptr)
                {
                    auto *glTexture = static_cast<OpenGLTexture *>(binding.texture);
                    textureId = glTexture->GetGLID();
                }
                if (binding.sampler != nullptr)
                {
                    auto *glSampler = static_cast<OpenGLSampler *>(binding.sampler);
                    samplerId = glSampler->GetGLID();
                }

                textureIds[count] = textureId;
                samplerIds[count] = samplerId;
                slots[count] = binding.slot;
                const std::uint32_t slot = binding.slot;
                if (slot >= m_TextureBindings.size() || m_TextureBindings[slot] != textureId)
                {
                    textureDirtySlots[count] = true;
                }
                if (slot >= m_SamplerBindings.size() || m_SamplerBindings[slot] != samplerId)
                {
                    samplerDirtySlots[count] = true;
                }
                ++count;
            }

            for (std::uint32_t i = 0; i < count;)
            {
                if (!textureDirtySlots[i])
                {
                    ++i;
                    continue;
                }

                const std::uint32_t dirtyOffset = i;
                std::uint32_t dirtyCount = 1u;
                while (dirtyOffset + dirtyCount < count && textureDirtySlots[dirtyOffset + dirtyCount])
                {
                    ++dirtyCount;
                }

                glBindTextures(slots[dirtyOffset], static_cast<GLsizei>(dirtyCount), textureIds.data() + dirtyOffset);
                m_Statistics.textureBinds += dirtyCount;
                submitted = true;
                for (std::uint32_t j = 0; j < dirtyCount; ++j)
                {
                    const std::uint32_t slot = slots[dirtyOffset + j];
                    if (slot < m_TextureBindings.size())
                    {
                        m_TextureBindings[slot] = textureIds[dirtyOffset + j];
                    }
                }
                i = dirtyOffset + dirtyCount;
            }

            for (std::uint32_t i = 0; i < count;)
            {
                if (!samplerDirtySlots[i])
                {
                    ++i;
                    continue;
                }

                const std::uint32_t dirtyOffset = i;
                std::uint32_t dirtyCount = 1u;
                while (dirtyOffset + dirtyCount < count && samplerDirtySlots[dirtyOffset + dirtyCount])
                {
                    ++dirtyCount;
                }

                glBindSamplers(slots[dirtyOffset], static_cast<GLsizei>(dirtyCount), samplerIds.data() + dirtyOffset);
                m_Statistics.samplerBinds += dirtyCount;
                submitted = true;
                for (std::uint32_t j = 0; j < dirtyCount; ++j)
                {
                    const std::uint32_t slot = slots[dirtyOffset + j];
                    if (slot < m_SamplerBindings.size())
                    {
                        m_SamplerBindings[slot] = samplerIds[dirtyOffset + j];
                    }
                }
                i = dirtyOffset + dirtyCount;
            }
            begin += count;
        }
        return submitted;
    }

    void OpenGLCommandList::SetStorageTexture(
        std::uint32_t slot,
        RHITexture *texture,
        std::uint32_t mipLevel,
        std::uint32_t arrayLayer,
        StorageTextureAccess access)
    {
        GLuint id = 0;
        GLenum internalFormat = GL_RGBA8;
        GLboolean layered = GL_FALSE;
        GLint layer = static_cast<GLint>(arrayLayer);
        if (texture != nullptr)
        {
            auto *glTex = static_cast<OpenGLTexture *>(texture);
            id = glTex->GetGLID();
            internalFormat = ToGLTextureFormat(glTex->GetFormat()).internalFormat;
            layered = glTex->GetDimension() == TextureDimension::TexCube ? GL_TRUE : GL_FALSE;
            layer = glTex->GetDimension() == TextureDimension::TexCube ? 0 : layer;
        }

        GLenum glAccess = GL_READ_WRITE;
        if (access == StorageTextureAccess::ReadOnly)
        {
            glAccess = GL_READ_ONLY;
        }
        else if (access == StorageTextureAccess::WriteOnly)
        {
            glAccess = GL_WRITE_ONLY;
        }

        if (slot < m_ImageTextureBindings.size())
        {
            ImageTextureBindingState &cached = m_ImageTextureBindings[slot];
            if (cached.valid &&
                cached.texture == id &&
                cached.mipLevel == static_cast<GLint>(mipLevel) &&
                cached.layered == layered &&
                cached.layer == layer &&
                cached.access == glAccess &&
                cached.internalFormat == internalFormat)
            {
                return;
            }
        }

        glBindImageTexture(
            slot,
            id,
            static_cast<GLint>(mipLevel),
            layered,
            layer,
            glAccess,
            internalFormat);
        ++m_Statistics.textureBinds;

        if (slot < m_ImageTextureBindings.size())
        {
            m_ImageTextureBindings[slot] = ImageTextureBindingState{
                id,
                static_cast<GLint>(mipLevel),
                layered,
                layer,
                glAccess,
                internalFormat,
                true};
        }
    }

    void OpenGLCommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
    {
        // Viewport和depth range是动态状态; OpenGL坐标原点在左下
        const bool viewportDirty =
            !m_ViewportState.valid ||
            m_ViewportState.x != x ||
            m_ViewportState.y != y ||
            m_ViewportState.width != width ||
            m_ViewportState.height != height;
        const bool depthRangeDirty =
            !m_ViewportState.valid ||
            m_ViewportState.minDepth != minDepth ||
            m_ViewportState.maxDepth != maxDepth;
        if (!viewportDirty && !depthRangeDirty)
        {
            return;
        }

        if (viewportDirty)
        {
            glViewport(
                static_cast<GLint>(x),
                static_cast<GLint>(y),
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height));
        }
        if (depthRangeDirty)
        {
            glDepthRangef(minDepth, maxDepth);
        }
        m_ViewportState = ViewportState{x, y, width, height, minDepth, maxDepth, true};
    }

    void OpenGLCommandList::SetScissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height)
    {
        // 当前RHI只有设置scissor, 没有禁用接口; 设置时确保GL_SCISSOR_TEST开启
        const bool coversViewport =
            m_ViewportState.valid &&
            x == static_cast<std::int32_t>(m_ViewportState.x) &&
            y == static_cast<std::int32_t>(m_ViewportState.y) &&
            width == static_cast<std::uint32_t>(m_ViewportState.width) &&
            height == static_cast<std::uint32_t>(m_ViewportState.height);
        if (coversViewport)
        {
            SetScissorEnabled(false);
            m_ScissorState = ScissorState{x, y, width, height, false, true};
            return;
        }

        const bool rectDirty =
            !m_ScissorState.valid ||
            m_ScissorState.x != x ||
            m_ScissorState.y != y ||
            m_ScissorState.width != width ||
            m_ScissorState.height != height;
        SetScissorEnabled(true);

        if (rectDirty)
        {
            glScissor(
                static_cast<GLint>(x),
                static_cast<GLint>(y),
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height));
        }
        m_ScissorState = ScissorState{x, y, width, height, true, true};
    }

    void OpenGLCommandList::PushConstants(ShaderStage stage, const void *data, std::uint32_t size, std::uint32_t offset)
    {
        // OpenGL fallback: 把小块常量写入固定大小UBO, 再绑定到slot 0
        // stage参数保留RHI语义, OpenGL UBO绑定本身不区分shader stage
        (void)stage;

        if (data == nullptr || size == 0)
        {
            return;
        }

        if (offset + size > kPushConstantsSize)
        {
            PHYSARA_CORE_ERROR("PushConstants overflow: size={} offset={}", size, offset);
            return;
        }

        // DSA路径更新UBO内容, 不依赖当前GL_UNIFORM_BUFFER绑定点; 然后绑定到slot 0供shader读取
        glNamedBufferSubData(
            m_PushConstantsBuffer,
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(size),
            data);

        BindRawBufferRange(
            GL_UNIFORM_BUFFER,
            m_UniformBufferBindings,
            m_Statistics.uniformBufferBinds,
            0,
            m_PushConstantsBuffer,
            offset,
            size);
    }

    void OpenGLCommandList::BeginRenderPass(
        RHIFramebuffer *framebuffer,
        const RHIRenderPassDesc &desc,
        std::span<const glm::vec4> clearColors,
        float clearDepth)
    {
        // OpenGL没有render pass对象. 这里把RHI RenderPassDesc翻译为:
        // 1) 绑定目标FBO; 2) 按loadOp清除attachment; 3)记录desc供EndRenderPass处理storeOp
        GLuint fboID = 0;
        if (framebuffer)
        {
            auto *glFbo = static_cast<OpenGLFramebuffer *>(framebuffer);
            fboID = glFbo->GetID();
        }

        BindFramebuffer(fboID);
        ++m_Statistics.renderPasses;

        m_CurrentPassDesc = &desc;

        const std::uint32_t colorCount = static_cast<std::uint32_t>(desc.colorAttachments.size());
        if (fboID == 0)
        {
            ConfigureDefaultDrawBuffers(colorCount);
        }

        bool needsClear = desc.hasDepth && desc.depthAttachment.loadOp == LoadOp::Clear;
        for (std::uint32_t i = 0; i < colorCount; ++i)
        {
            if (desc.colorAttachments[i].loadOp == LoadOp::Clear)
            {
                SetColorMask(i, true, true, true, true);
                needsClear = true;
            }
        }
        if (desc.hasDepth && desc.depthAttachment.loadOp == LoadOp::Clear)
        {
            SetDepthMaskState(true);
            if (OpenGLCommandListDetail::IsDepthStencilFormat(desc.depthAttachment.format))
            {
                SetStencilMaskState(0xffffffffu);
            }
        }

        if (needsClear)
        {
            SetScissorEnabled(false);
        }
        for (std::uint32_t i = 0; i < colorCount; ++i)
        {
            if (desc.colorAttachments[i].loadOp != LoadOp::Clear)
            {
                continue;
            }

            glm::vec4 color(0.f);
            if (i < clearColors.size())
            {
                color = clearColors[i];
            }

            if (fboID != 0)
            {
                // DSA清FBO attachment, 不依赖当前GL_FRAMEBUFFER绑定点
                glClearNamedFramebufferfv(fboID, GL_COLOR, static_cast<GLint>(i), &color.x);
            }
            else
            {
                // 直接glClear清默认帧缓冲, 只支持清第0个color attachment
                glClearBufferfv(GL_COLOR, static_cast<GLint>(i), &color.x);
            }
            ++m_Statistics.clears;
        }

        if (desc.hasDepth && desc.depthAttachment.loadOp == LoadOp::Clear)
        {
            if (OpenGLCommandListDetail::IsDepthStencilFormat(desc.depthAttachment.format))
            {
                if (fboID != 0)
                {
                    glClearNamedFramebufferfi(fboID, GL_DEPTH_STENCIL, 0, clearDepth, 0);
                }
                else
                {
                    glClearBufferfi(GL_DEPTH_STENCIL, 0, clearDepth, 0);
                }
                ++m_Statistics.clears;
            }
            else
            {
                if (fboID != 0)
                {
                    glClearNamedFramebufferfv(fboID, GL_DEPTH, 0, &clearDepth);
                }
                else
                {
                    glClearBufferfv(GL_DEPTH, 0, &clearDepth);
                }
                ++m_Statistics.clears;
            }
        }
    }

    void OpenGLCommandList::EndRenderPass()
    {
        // storeOp=DontCare的attachment用invalidate告诉驱动内容不再需要,
        // tile-based或带压缩的实现可以避免无意义store/resolve
        if (!m_CurrentPassDesc)
        {
            return;
        }

        if (m_FramebufferBinding.valid && m_FramebufferBinding.framebuffer != 0)
        {
            std::vector<GLenum> attachments;

            const std::uint32_t colorCount = static_cast<std::uint32_t>(m_CurrentPassDesc->colorAttachments.size());
            for (std::uint32_t i = 0; i < colorCount; ++i)
            {
                if (m_CurrentPassDesc->colorAttachments[i].storeOp == StoreOp::DontCare)
                {
                    attachments.push_back(ToGLAttachmentPoint(i));
                }
            }

            if (m_CurrentPassDesc->hasDepth && m_CurrentPassDesc->depthAttachment.storeOp == StoreOp::DontCare)
            {
                attachments.push_back(
                    OpenGLCommandListDetail::IsDepthStencilFormat(m_CurrentPassDesc->depthAttachment.format)
                        ? GL_DEPTH_STENCIL_ATTACHMENT
                        : GL_DEPTH_ATTACHMENT);
            }

            if (!attachments.empty())
            {
                // DSA路径invalidate framebuffer, 不依赖当前GL_FRAMEBUFFER绑定点; 也可以直接glInvalidateFramebuffer, 但需要再次指定target
                glInvalidateNamedFramebufferData(
                    m_FramebufferBinding.framebuffer,
                    static_cast<GLsizei>(attachments.size()),
                    attachments.data());
            }
        }

        m_CurrentPassDesc = nullptr;
    }

    void OpenGLCommandList::DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t instanceCount,
        std::uint32_t firstIndex,
        std::int32_t vertexOffset,
        std::uint32_t firstInstance)
    {
        // 现代indexed draw: 一次调用同时支持instancing、baseVertex、baseInstance
        // firstIndex和SetIndexBuffer(offset) 都以byte offset合并传入indices参数
        const std::uint32_t indexStride = OpenGLCommandListDetail::GetIndexStride(m_State.indexType);
        const std::uintptr_t offset =
            static_cast<std::uintptr_t>(m_State.indexOffset) +
            static_cast<std::uintptr_t>(firstIndex) * indexStride;

        // 非常规draw, 需要同时指定instance count、base vertex和base instance; 方便后续object data用gl_InstanceID/baseInstance索引
        glDrawElementsInstancedBaseVertexBaseInstance(
            m_State.topology,
            static_cast<GLsizei>(indexCount),
            m_State.indexType,
            reinterpret_cast<const void *>(offset),
            static_cast<GLsizei>(instanceCount),
            vertexOffset,
            firstInstance);
        ++m_Statistics.drawCalls;
    }

    void OpenGLCommandList::Draw(
        std::uint32_t vertexCount,
        std::uint32_t instanceCount,
        std::uint32_t firstVertex,
        std::uint32_t firstInstance)
    {
        // 非索引draw, 同样使用baseInstance版本, 方便后续object data用gl_InstanceID/baseInstance索引
        glDrawArraysInstancedBaseInstance(
            m_State.topology,
            static_cast<GLint>(firstVertex),
            static_cast<GLsizei>(vertexCount),
            static_cast<GLsizei>(instanceCount),
            firstInstance);
        ++m_Statistics.drawCalls;
    }

    void OpenGLCommandList::Dispatch(std::uint32_t groupX, std::uint32_t groupY, std::uint32_t groupZ)
    {
        // Compute dispatch只提交workgroup数量; 资源可见性由后续BufferBarrier/TextureBarrier控制
        glDispatchCompute(groupX, groupY, groupZ);
        ++m_Statistics.dispatchCalls;
    }

    void OpenGLCommandList::DrawIndexedIndirect(
        RHIBuffer *indirectBuffer,
        std::uint32_t drawCount,
        std::uint32_t stride,
        std::uint32_t offset)
    {
        // MDI: GPU/CPU准备一组DrawElementsIndirectCommand,
        // glMultiDrawElementsIndirect一次提交多draw, 减少CPU driver overhead
        auto *glBuffer = static_cast<OpenGLBuffer *>(indirectBuffer);
        if (!glBuffer)
        {
            PHYSARA_CORE_ERROR("DrawIndexedIndirect called with null buffer.");
            return;
        }
        if (drawCount == 0)
        {
            return;
        }

        if (m_State.indexOffset != 0)
        {
            PHYSARA_CORE_WARN("DrawIndexedIndirect ignores indexOffset for now.");
        }

        const GLuint bufferId = glBuffer->GetGLID();
        if (!m_IndirectBufferBinding.valid || m_IndirectBufferBinding.buffer != bufferId)
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, bufferId);
            ++m_Statistics.indirectBufferBinds;
            m_IndirectBufferBinding = IndirectBufferBindingState{bufferId, true};
        }

        const std::uintptr_t indirectOffset =
            static_cast<std::uintptr_t>(glBuffer->GetBindOffset()) + static_cast<std::uintptr_t>(offset);
        glMultiDrawElementsIndirect(
            m_State.topology,
            m_State.indexType,
            reinterpret_cast<const void *>(indirectOffset),
            static_cast<GLsizei>(drawCount),
            static_cast<GLsizei>(stride));
        ++m_Statistics.indirectDrawCalls;
        m_Statistics.indirectDrawCommands += drawCount;
    }

    void OpenGLCommandList::TextureBarrier(RHITexture *texture, ShaderStage srcStage, ShaderStage dstStage)
    {
        // 旧接口保守同步: 适合纹理更新、image store、后续采样之间的通用可见性
        (void)texture;
        (void)srcStage;
        (void)dstStage;

        glMemoryBarrier(
            GL_TEXTURE_FETCH_BARRIER_BIT |
            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
            GL_TEXTURE_UPDATE_BARRIER_BIT);
        ++m_Statistics.barriers;
    }

    void OpenGLCommandList::BufferBarrier(RHIBuffer *buffer, ShaderStage srcStage, ShaderStage dstStage)
    {
        // 旧接口保守同步: 覆盖SSBO写入和buffer update后续可见性
        (void)buffer;
        (void)srcStage;
        (void)dstStage;

        glMemoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT);
        ++m_Statistics.barriers;
    }

    void OpenGLCommandList::TextureBarrier(RHITexture *texture, const RHIResourceBarrier &barrier)
    {
        // 新接口按RHIResourceBarrier映射barrier bits, 后续RenderGraph可直接走这里
        (void)texture;
        const GLbitfield bits = OpenGLCommandListDetail::ToGLMemoryBarrierBits(barrier);
        if (bits != 0)
        {
            glMemoryBarrier(bits);
            ++m_Statistics.barriers;
        }
    }

    void OpenGLCommandList::BufferBarrier(RHIBuffer *buffer, const RHIResourceBarrier &barrier)
    {
        (void)buffer;
        const GLbitfield bits = OpenGLCommandListDetail::ToGLMemoryBarrierBits(barrier);
        if (bits != 0)
        {
            glMemoryBarrier(bits);
            ++m_Statistics.barriers;
        }
    }

    void OpenGLCommandList::CopyTextureToTexture(RHITexture *src, RHITexture *dst)
    {
        // DSA image copy: 跨texture object直接拷贝, 不需要绑定FBO或纹理到context
        auto *glSrc = static_cast<OpenGLTexture *>(src);
        auto *glDst = static_cast<OpenGLTexture *>(dst);
        if (!glSrc || !glDst)
        {
            PHYSARA_CORE_ERROR("CopyTextureToTexture called with null texture.");
            return;
        }

        const std::uint32_t width = std::min(glSrc->GetWidth(), glDst->GetWidth());
        const std::uint32_t height = std::min(glSrc->GetHeight(), glDst->GetHeight());

        glCopyImageSubData(
            glSrc->GetGLID(),
            glSrc->GetGLTarget(),
            0,
            0,
            0,
            0,
            glDst->GetGLID(),
            glDst->GetGLTarget(),
            0,
            0,
            0,
            0,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            1);
    }

    void OpenGLCommandList::ResolveTexture(RHITexture *src, RHITexture *dst)
    {
        auto *glSrc = static_cast<OpenGLTexture *>(src);
        auto *glDst = static_cast<OpenGLTexture *>(dst);
        if (!glSrc || !glDst)
        {
            PHYSARA_CORE_ERROR("ResolveTexture called with null texture.");
            return;
        }

        if (m_ResolveReadFramebuffer == 0 || m_ResolveDrawFramebuffer == 0)
        {
            PHYSARA_CORE_ERROR("ResolveTexture called before resolve framebuffers were initialized.");
            return;
        }

        const bool depth = glSrc->GetFormat() == TextureFormat::Depth24Stencil8 || glSrc->GetFormat() == TextureFormat::Depth32F;
        const GLenum attachment = depth
                                      ? (glSrc->GetFormat() == TextureFormat::Depth24Stencil8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT)
                                      : GL_COLOR_ATTACHMENT0;
        glNamedFramebufferTexture(m_ResolveReadFramebuffer, attachment, glSrc->GetGLID(), 0);
        glNamedFramebufferTexture(m_ResolveDrawFramebuffer, attachment, glDst->GetGLID(), 0);

        if (!depth)
        {
            const GLenum colorAttachment = GL_COLOR_ATTACHMENT0;
            glNamedFramebufferDrawBuffers(m_ResolveDrawFramebuffer, 1, &colorAttachment);
            glNamedFramebufferReadBuffer(m_ResolveReadFramebuffer, GL_COLOR_ATTACHMENT0);
        }
        else
        {
            glNamedFramebufferDrawBuffers(m_ResolveDrawFramebuffer, 0, nullptr);
            glNamedFramebufferReadBuffer(m_ResolveReadFramebuffer, GL_NONE);
        }

        const GLbitfield mask = depth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;
        glBlitNamedFramebuffer(
            m_ResolveReadFramebuffer,
            m_ResolveDrawFramebuffer,
            0,
            0,
            static_cast<GLint>(std::min(glSrc->GetWidth(), glDst->GetWidth())),
            static_cast<GLint>(std::min(glSrc->GetHeight(), glDst->GetHeight())),
            0,
            0,
            static_cast<GLint>(std::min(glSrc->GetWidth(), glDst->GetWidth())),
            static_cast<GLint>(std::min(glSrc->GetHeight(), glDst->GetHeight())),
            mask,
            GL_NEAREST);
        ++m_Statistics.resolves;
        glNamedFramebufferTexture(m_ResolveReadFramebuffer, attachment, 0, 0);
        glNamedFramebufferTexture(m_ResolveDrawFramebuffer, attachment, 0, 0);
    }

    void OpenGLCommandList::CopyBufferToTexture(RHIBuffer *src, RHITexture *dst)
    {
        // 通过PBO(GL_PIXEL_UNPACK_BUFFER)上传到texture, 这里data=nullptr表示从当前PBO的offset 0读取
        auto *glBuffer = static_cast<OpenGLBuffer *>(src);
        auto *glTex = static_cast<OpenGLTexture *>(dst);
        if (!glBuffer || !glTex)
        {
            PHYSARA_CORE_ERROR("CopyBufferToTexture called with null resource.");
            return;
        }

        if (glTex->GetGLTarget() != GL_TEXTURE_2D)
        {
            PHYSARA_CORE_WARN("CopyBufferToTexture supports GL_TEXTURE_2D only.");
            return;
        }

        const auto fmt = ToGLTextureFormat(glTex->GetFormat());

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glBuffer->GetGLID());
        glTextureSubImage2D(
            glTex->GetGLID(),
            0,
            0,
            0,
            static_cast<GLsizei>(glTex->GetWidth()),
            static_cast<GLsizei>(glTex->GetHeight()),
            fmt.baseFormat,
            fmt.type,
            nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void OpenGLCommandList::GenerateMipmaps(RHITexture *texture)
    {
        // DSA mip生成, 不需要glBindTexture
        auto *glTex = static_cast<OpenGLTexture *>(texture);
        if (!glTex)
        {
            PHYSARA_CORE_ERROR("GenerateMipmaps called with null texture.");
            return;
        }

        glGenerateTextureMipmap(glTex->GetGLID());
        ++m_Statistics.mipmapGenerates;
    }

    std::vector<std::uint8_t> OpenGLCommandList::ReadTextureToCPU(RHITexture *texture, const RHITextureReadbackDesc &desc)
    {
        auto *glTex = static_cast<OpenGLTexture *>(texture);
        if (!glTex)
        {
            PHYSARA_CORE_ERROR("ReadTextureToCPU called with null texture.");
            return {};
        }

        if (glTex->GetGLTarget() != GL_TEXTURE_2D)
        {
            PHYSARA_CORE_WARN("ReadTextureToCPU supports GL_TEXTURE_2D only.");
            return {};
        }

        if (desc.format != glTex->GetFormat())
        {
            PHYSARA_CORE_WARN("ReadTextureToCPU format mismatch.");
            return {};
        }

        const std::uint32_t readWidth = desc.width != 0 ? desc.width : glTex->GetWidth();
        const std::uint32_t readHeight = desc.height != 0 ? desc.height : glTex->GetHeight();
        if (readWidth == 0 || readHeight == 0)
        {
            return {};
        }

        const auto fmt = ToGLTextureFormat(desc.format);
        std::uint32_t channelCount = 4u;
        std::uint32_t bytesPerChannel = 1u;
        if (desc.format == RHI::TextureFormat::RG16F)
        {
            channelCount = 2u;
            bytesPerChannel = sizeof(float);
        }
        else if (desc.format == RHI::TextureFormat::RGBA16F)
        {
            channelCount = 4u;
            bytesPerChannel = sizeof(float);
        }
        else if (desc.format == RHI::TextureFormat::RGBA32F)
        {
            channelCount = 4u;
            bytesPerChannel = sizeof(float);
        }
        else if (desc.format != RHI::TextureFormat::RGBA8)
        {
            PHYSARA_CORE_WARN("ReadTextureToCPU unsupported format.");
            return {};
        }
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(readWidth) * readHeight * channelCount * bytesPerChannel);

        GLint previousPackAlignment = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
        glPixelStorei(GL_PACK_ALIGNMENT, 1); // 以字节为单位紧密存储, 避免默认4字节对齐导致的行末padding

        glGetTextureSubImage(
            glTex->GetGLID(),
            static_cast<GLint>(desc.mipLevel),
            static_cast<GLint>(desc.x),
            static_cast<GLint>(desc.y),
            static_cast<GLint>(desc.arrayLayer),
            static_cast<GLsizei>(readWidth),
            static_cast<GLsizei>(readHeight),
            1,
            fmt.baseFormat,
            fmt.type,
            static_cast<GLsizei>(pixels.size()),
            pixels.data());
        ++m_Statistics.readbacks;

        glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);// 恢复之前的pack alignment状态
        return pixels;
    }

    void OpenGLCommandList::BeginDebugLabel(const char *label)
    {
        // KHR_debug debug group, RenderDoc/Nsight中会显示为命令分组
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label ? label : "");
    }

    void OpenGLCommandList::EndDebugLabel()
    {
        glPopDebugGroup();
    }

    void OpenGLCommandList::InvalidateExternalState()
    {
        InvalidateBindingCache();
        InvalidatePipelineState();
        InvalidateDynamicStateCache();
        InvalidateRenderPassStateCache();
        m_CurrentPassDesc = nullptr;
        m_CurrentPipelineDesc = nullptr;
    }

    void OpenGLCommandList::InvalidateImGuiState()
    {
        m_PipelineStateValid = false;
        m_State.program = 0;
        m_State.vao = 0;
        InvalidateVertexInputCache();
        InvalidateDynamicStateCache();
        InvalidateRenderPassStateCache();
        if (!m_TextureBindings.empty())
        {
            m_TextureBindings[0] = std::numeric_limits<GLuint>::max();
        }
        if (!m_SamplerBindings.empty())
        {
            m_SamplerBindings[0] = std::numeric_limits<GLuint>::max();
        }
        m_CurrentPassDesc = nullptr;
        m_CurrentPipelineDesc = nullptr;
    }
}