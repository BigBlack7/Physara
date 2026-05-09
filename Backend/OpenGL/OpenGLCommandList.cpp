#include "OpenGLCommandList.hpp"

#include <algorithm>
#include <cstdint>
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
    namespace Internal
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
            GLbitfield bits = 0;

            auto hasBeforeOrAfter = [&barrier](ResourceState state)
            {
                return barrier.before == state || barrier.after == state;
            };

            if ((barrier.dstAccess & ResourceAccess::ShaderRead) != 0u ||
                (barrier.srcAccess & ResourceAccess::ShaderWrite) != 0u ||
                hasBeforeOrAfter(ResourceState::ShaderResource))
            {
                bits |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::ShaderWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::ShaderWrite) != 0u ||
                hasBeforeOrAfter(ResourceState::UnorderedAccess))
            {
                bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::VertexAttributeRead) != 0u ||
                hasBeforeOrAfter(ResourceState::VertexBuffer))
            {
                bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::IndexRead) != 0u ||
                hasBeforeOrAfter(ResourceState::IndexBuffer))
            {
                bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
            }

            if ((barrier.dstAccess & ResourceAccess::UniformRead) != 0u ||
                hasBeforeOrAfter(ResourceState::ConstantBuffer))
            {
                bits |= GL_UNIFORM_BARRIER_BIT;
            }

            if ((barrier.srcAccess & (ResourceAccess::TransferWrite | ResourceAccess::ColorAttachmentWrite)) != 0u ||
                (barrier.dstAccess & (ResourceAccess::TransferRead | ResourceAccess::TransferWrite)) != 0u ||
                hasBeforeOrAfter(ResourceState::CopySource) ||
                hasBeforeOrAfter(ResourceState::CopyDest))
            {
                bits |= GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::ColorAttachmentWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::ColorAttachmentRead) != 0u ||
                hasBeforeOrAfter(ResourceState::RenderTarget))
            {
                bits |= GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::DepthStencilWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::DepthStencilRead) != 0u ||
                hasBeforeOrAfter(ResourceState::DepthWrite) ||
                hasBeforeOrAfter(ResourceState::DepthRead))
            {
                bits |= GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;
            }

            if ((barrier.srcAccess & ResourceAccess::HostWrite) != 0u ||
                (barrier.dstAccess & ResourceAccess::HostRead) != 0u)
            {
                bits |= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
            }

            return bits != 0 ? bits : GL_ALL_BARRIER_BITS;
        }
    }

    OpenGLCommandList::OpenGLCommandList()
    {
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
    }

    OpenGLCommandList::~OpenGLCommandList()
    {
        if (m_PushConstantsBuffer != 0)
        {
            glDeleteBuffers(1, &m_PushConstantsBuffer);
            m_PushConstantsBuffer = 0;
        }
    }

    void OpenGLCommandList::SetPipelineState(RHIPipelineState *pso)
    {
        auto *gl = static_cast<OpenGLPipeline *>(pso);
        if (gl == nullptr || !gl->IsValid())
        {
            PHYSARA_CORE_ERROR("Invalid pipeline state.");
            return;
        }

        if (m_State.program != gl->GetProgram())
        {
            glUseProgram(gl->GetProgram());
            m_State.program = gl->GetProgram();
        }

        if (!gl->IsCompute() && m_State.vao != gl->GetVAO())
        {
            glBindVertexArray(gl->GetVAO());
            m_State.vao = gl->GetVAO();
        }

        const auto &desc = gl->GetDesc();
        m_CurrentPipelineDesc = &desc;

        if (gl->IsCompute())
        {
            return;
        }

        if (m_State.cullMode != desc.rasterizerState.cullMode)
        {
            if (desc.rasterizerState.cullMode == CullMode::None)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(ToGLCullMode(desc.rasterizerState.cullMode));
            }
            m_State.cullMode = desc.rasterizerState.cullMode;
        }

        if (m_State.polygonMode != desc.rasterizerState.polygonMode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, ToGLPolygonMode(desc.rasterizerState.polygonMode));
            m_State.polygonMode = desc.rasterizerState.polygonMode;
        }

        if (m_State.depthBias != desc.rasterizerState.depthBias ||
            m_State.depthBiasSlope != desc.rasterizerState.depthBiasSlope)
        {
            if (desc.rasterizerState.depthBias != 0.f || desc.rasterizerState.depthBiasSlope != 0.f)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(desc.rasterizerState.depthBiasSlope, desc.rasterizerState.depthBias);
            }
            else
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            m_State.depthBias = desc.rasterizerState.depthBias;
            m_State.depthBiasSlope = desc.rasterizerState.depthBiasSlope;
        }

        if (m_State.depthTest != desc.depthStencilState.depthTest)
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

        if (m_State.depthWrite != desc.depthStencilState.depthWrite)
        {
            glDepthMask(desc.depthStencilState.depthWrite ? GL_TRUE : GL_FALSE);
            m_State.depthWrite = desc.depthStencilState.depthWrite;
        }

        if (m_State.depthFunc != desc.depthStencilState.compareOp)
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

            if (!Internal::BlendStateEqual(m_State.blendStates[i], target))
            {
                if (target.blendEnable)
                {
                    glEnablei(GL_BLEND, i);
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
                else
                {
                    glDisablei(GL_BLEND, i);
                }

                m_State.blendStates[i] = target;
            }
        }

        m_State.topology = ToGLTopology(desc.topology);
    }

    void OpenGLCommandList::SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset)
    {
        if (m_State.vao == 0)
        {
            PHYSARA_CORE_ERROR("SetVertexBuffer called without a bound VAO.");
            return;
        }

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

        glVertexArrayVertexBuffer(
            m_State.vao,
            binding,
            id,
            static_cast<GLintptr>(offset),
            static_cast<GLsizei>(stride));
    }

    void OpenGLCommandList::SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset)
    {
        if (m_State.vao == 0)
        {
            PHYSARA_CORE_ERROR("SetIndexBuffer called without a bound VAO.");
            return;
        }

        GLuint id = 0;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            id = glBuffer->GetGLID();
        }

        glVertexArrayElementBuffer(m_State.vao, id);
        m_State.indexOffset = offset;
        m_State.indexType = GL_UNSIGNED_INT;
    }

    void OpenGLCommandList::SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer)
    {
        GLuint id = 0;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            id = glBuffer->GetGLID();
        }

        glBindBufferBase(GL_UNIFORM_BUFFER, slot, id);
    }

    void OpenGLCommandList::SetTexture(std::uint32_t slot, RHITexture *texture, RHISampler *sampler)
    {
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

        glBindTextureUnit(slot, texID);
        glBindSampler(slot, samplerID);
    }

    void OpenGLCommandList::SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer)
    {
        GLuint id = 0;
        if (buffer)
        {
            auto *glBuffer = static_cast<OpenGLBuffer *>(buffer);
            id = glBuffer->GetGLID();
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, id);
    }

    void OpenGLCommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
    {
        glViewport(
            static_cast<GLint>(x),
            static_cast<GLint>(y),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height));
        glDepthRangef(minDepth, maxDepth);
    }

    void OpenGLCommandList::SetScissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            static_cast<GLint>(x),
            static_cast<GLint>(y),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height));
    }

    void OpenGLCommandList::PushConstants(ShaderStage stage, const void *data, std::uint32_t size, std::uint32_t offset)
    {
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

        glNamedBufferSubData(
            m_PushConstantsBuffer,
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(size),
            data);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            0,
            m_PushConstantsBuffer,
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(size));
    }

    void OpenGLCommandList::BeginRenderPass(
        RHIFramebuffer *framebuffer,
        const RHIRenderPassDesc &desc,
        const std::vector<glm::vec4> &clearColors,
        float clearDepth)
    {
        GLuint fboID = 0;
        if (framebuffer)
        {
            auto *glFbo = static_cast<OpenGLFramebuffer *>(framebuffer);
            fboID = glFbo->GetID();
        }

        if (m_State.framebuffer != fboID)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fboID);
            m_State.framebuffer = fboID;
        }

        m_CurrentPassDesc = &desc;

        const std::uint32_t colorCount = static_cast<std::uint32_t>(desc.colorAttachments.size());
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
                glClearNamedFramebufferfv(fboID, GL_COLOR, static_cast<GLint>(i), &color.x);
            }
            else
            {
                glClearBufferfv(GL_COLOR, static_cast<GLint>(i), &color.x);
            }
        }

        if (desc.hasDepth && desc.depthAttachment.loadOp == LoadOp::Clear)
        {
            if (Internal::IsDepthStencilFormat(desc.depthAttachment.format))
            {
                if (fboID != 0)
                {
                    glClearNamedFramebufferfi(fboID, GL_DEPTH_STENCIL, 0, clearDepth, 0);
                }
                else
                {
                    glClearBufferfi(GL_DEPTH_STENCIL, 0, clearDepth, 0);
                }
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
            }
        }
    }

    void OpenGLCommandList::EndRenderPass()
    {
        if (!m_CurrentPassDesc)
        {
            return;
        }

        if (m_State.framebuffer != 0)
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
                    Internal::IsDepthStencilFormat(m_CurrentPassDesc->depthAttachment.format)
                        ? GL_DEPTH_STENCIL_ATTACHMENT
                        : GL_DEPTH_ATTACHMENT);
            }

            if (!attachments.empty())
            {
                glInvalidateNamedFramebufferData(
                    m_State.framebuffer,
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
        const std::uint32_t indexStride = Internal::GetIndexStride(m_State.indexType);
        const std::uintptr_t offset =
            static_cast<std::uintptr_t>(m_State.indexOffset) +
            static_cast<std::uintptr_t>(firstIndex) * indexStride;

        glDrawElementsInstancedBaseVertexBaseInstance(
            m_State.topology,
            static_cast<GLsizei>(indexCount),
            m_State.indexType,
            reinterpret_cast<const void *>(offset),
            static_cast<GLsizei>(instanceCount),
            vertexOffset,
            firstInstance);
    }

    void OpenGLCommandList::Draw(
        std::uint32_t vertexCount,
        std::uint32_t instanceCount,
        std::uint32_t firstVertex,
        std::uint32_t firstInstance)
    {
        glDrawArraysInstancedBaseInstance(
            m_State.topology,
            static_cast<GLint>(firstVertex),
            static_cast<GLsizei>(vertexCount),
            static_cast<GLsizei>(instanceCount),
            firstInstance);
    }

    void OpenGLCommandList::Dispatch(std::uint32_t groupX, std::uint32_t groupY, std::uint32_t groupZ)
    {
        glDispatchCompute(groupX, groupY, groupZ);
    }

    void OpenGLCommandList::DrawIndexedIndirect(RHIBuffer *indirectBuffer, std::uint32_t drawCount, std::uint32_t stride)
    {
        auto *glBuffer = static_cast<OpenGLBuffer *>(indirectBuffer);
        if (!glBuffer)
        {
            PHYSARA_CORE_ERROR("DrawIndexedIndirect called with null buffer.");
            return;
        }

        if (m_State.indexOffset != 0)
        {
            PHYSARA_CORE_WARN("DrawIndexedIndirect ignores indexOffset for now.");
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, glBuffer->GetGLID());
        glMultiDrawElementsIndirect(
            m_State.topology,
            m_State.indexType,
            nullptr,
            static_cast<GLsizei>(drawCount),
            static_cast<GLsizei>(stride));
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void OpenGLCommandList::TextureBarrier(RHITexture *texture, ShaderStage srcStage, ShaderStage dstStage)
    {
        (void)texture;
        (void)srcStage;
        (void)dstStage;

        glMemoryBarrier(
            GL_TEXTURE_FETCH_BARRIER_BIT |
            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
            GL_TEXTURE_UPDATE_BARRIER_BIT);
    }

    void OpenGLCommandList::BufferBarrier(RHIBuffer *buffer, ShaderStage srcStage, ShaderStage dstStage)
    {
        (void)buffer;
        (void)srcStage;
        (void)dstStage;

        glMemoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT);
    }

    void OpenGLCommandList::TextureBarrier(RHITexture *texture, const RHIResourceBarrier &barrier)
    {
        (void)texture;
        glMemoryBarrier(Internal::ToGLMemoryBarrierBits(barrier));
    }

    void OpenGLCommandList::BufferBarrier(RHIBuffer *buffer, const RHIResourceBarrier &barrier)
    {
        (void)buffer;
        glMemoryBarrier(Internal::ToGLMemoryBarrierBits(barrier));
    }

    void OpenGLCommandList::CopyTextureToTexture(RHITexture *src, RHITexture *dst)
    {
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

    void OpenGLCommandList::CopyBufferToTexture(RHIBuffer *src, RHITexture *dst)
    {
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
        auto *glTex = static_cast<OpenGLTexture *>(texture);
        if (!glTex)
        {
            PHYSARA_CORE_ERROR("GenerateMipmaps called with null texture.");
            return;
        }

        glGenerateTextureMipmap(glTex->GetGLID());
    }

    void OpenGLCommandList::BeginDebugLabel(const char *label)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label ? label : "");
    }

    void OpenGLCommandList::EndDebugLabel()
    {
        glPopDebugGroup();
    }
}