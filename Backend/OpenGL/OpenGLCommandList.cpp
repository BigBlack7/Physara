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

        if (m_state.program != gl->GetProgram())
        {
            glUseProgram(gl->GetProgram());
            m_state.program = gl->GetProgram();
        }

        if (!gl->IsCompute() && m_state.vao != gl->GetVAO())
        {
            glBindVertexArray(gl->GetVAO());
            m_state.vao = gl->GetVAO();
        }

        const auto &desc = gl->GetDesc();
        m_currentPipelineDesc = &desc;

        if (gl->IsCompute())
        {
            return;
        }

        if (m_state.cullMode != desc.rasterizerState.cullMode)
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
            m_state.cullMode = desc.rasterizerState.cullMode;
        }

        if (m_state.polygonMode != desc.rasterizerState.polygonMode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, ToGLPolygonMode(desc.rasterizerState.polygonMode));
            m_state.polygonMode = desc.rasterizerState.polygonMode;
        }

        if (m_state.depthBias != desc.rasterizerState.depthBias ||
            m_state.depthBiasSlope != desc.rasterizerState.depthBiasSlope)
        {
            if (desc.rasterizerState.depthBias != 0.0f || desc.rasterizerState.depthBiasSlope != 0.0f)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(desc.rasterizerState.depthBiasSlope, desc.rasterizerState.depthBias);
            }
            else
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            m_state.depthBias = desc.rasterizerState.depthBias;
            m_state.depthBiasSlope = desc.rasterizerState.depthBiasSlope;
        }

        if (m_state.depthTest != desc.depthStencilState.depthTest)
        {
            if (desc.depthStencilState.depthTest)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
            m_state.depthTest = desc.depthStencilState.depthTest;
        }

        if (m_state.depthWrite != desc.depthStencilState.depthWrite)
        {
            glDepthMask(desc.depthStencilState.depthWrite ? GL_TRUE : GL_FALSE);
            m_state.depthWrite = desc.depthStencilState.depthWrite;
        }

        if (m_state.depthFunc != desc.depthStencilState.compareOp)
        {
            glDepthFunc(ToGLDepthFunc(desc.depthStencilState.compareOp));
            m_state.depthFunc = desc.depthStencilState.compareOp;
        }

        for (std::uint32_t i = 0; i < kMaxColorAttachments; ++i)
        {
            RHIBlendState target{};
            if (i < desc.blendStates.size())
            {
                target = desc.blendStates[i];
            }

            if (!Internal::BlendStateEqual(m_state.blendStates[i], target))
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

                m_state.blendStates[i] = target;
            }
        }

        m_state.topology = ToGLTopology(desc.topology);
    }

    void OpenGLCommandList::SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset)
    {
        if (m_state.vao == 0)
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
        if (m_currentPipelineDesc)
        {
            for (const auto &b : m_currentPipelineDesc->vertexBindings)
            {
                if (b.binding == binding)
                {
                    stride = b.stride;
                    break;
                }
            }
        }

        glVertexArrayVertexBuffer(
            m_state.vao,
            binding,
            id,
            static_cast<GLintptr>(offset),
            static_cast<GLsizei>(stride));
    }

    void OpenGLCommandList::SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset)
    {
        if (m_state.vao == 0)
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

        glVertexArrayElementBuffer(m_state.vao, id);
        m_state.indexOffset = offset;
        m_state.indexType = GL_UNSIGNED_INT;
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

        if (m_state.framebuffer != fboID)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fboID);
            m_state.framebuffer = fboID;
        }

        m_currentPassDesc = &desc;

        const std::uint32_t colorCount = static_cast<std::uint32_t>(desc.colorAttachments.size());
        for (std::uint32_t i = 0; i < colorCount; ++i)
        {
            if (desc.colorAttachments[i].loadOp != LoadOp::Clear)
            {
                continue;
            }

            glm::vec4 color(0.0f);
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
        if (!m_currentPassDesc)
        {
            return;
        }

        if (m_state.framebuffer != 0)
        {
            std::vector<GLenum> attachments;

            const std::uint32_t colorCount = static_cast<std::uint32_t>(m_currentPassDesc->colorAttachments.size());
            for (std::uint32_t i = 0; i < colorCount; ++i)
            {
                if (m_currentPassDesc->colorAttachments[i].storeOp == StoreOp::DontCare)
                {
                    attachments.push_back(ToGLAttachmentPoint(i));
                }
            }

            if (m_currentPassDesc->hasDepth && m_currentPassDesc->depthAttachment.storeOp == StoreOp::DontCare)
            {
                attachments.push_back(
                    Internal::IsDepthStencilFormat(m_currentPassDesc->depthAttachment.format)
                        ? GL_DEPTH_STENCIL_ATTACHMENT
                        : GL_DEPTH_ATTACHMENT);
            }

            if (!attachments.empty())
            {
                glInvalidateNamedFramebufferData(
                    m_state.framebuffer,
                    static_cast<GLsizei>(attachments.size()),
                    attachments.data());
            }
        }

        m_currentPassDesc = nullptr;
    }

    void OpenGLCommandList::DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t instanceCount,
        std::uint32_t firstIndex,
        std::int32_t vertexOffset,
        std::uint32_t firstInstance)
    {
        const std::uint32_t indexStride = Internal::GetIndexStride(m_state.indexType);
        const std::uintptr_t offset =
            static_cast<std::uintptr_t>(m_state.indexOffset) +
            static_cast<std::uintptr_t>(firstIndex) * indexStride;

        glDrawElementsInstancedBaseVertexBaseInstance(
            m_state.topology,
            static_cast<GLsizei>(indexCount),
            m_state.indexType,
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
            m_state.topology,
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

        if (m_state.indexOffset != 0)
        {
            PHYSARA_CORE_WARN("DrawIndexedIndirect ignores indexOffset for now.");
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, glBuffer->GetGLID());
        glMultiDrawElementsIndirect(
            m_state.topology,
            m_state.indexType,
            nullptr,
            static_cast<GLsizei>(drawCount),
            static_cast<GLsizei>(stride));
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