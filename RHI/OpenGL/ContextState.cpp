#include "ContextState.hpp"

/// @file
/// @brief 创建有界 context 辅助对象, 统一缓存绑定与顶点格式, 删除前解除 native 引用.
/// 普通绘制, 传输和窗口末端复制共用此状态管理器.

#include <algorithm>
#include <span>

#include "Backend.hpp"
#include "InputLayout.hpp"

namespace Physara::RHI::OpenGL
{
    bool ContextState::Initialize(OpenGLDevice &device)
    {
        // 先核对该 profile 实际使用的图形和末端复制入口, 再分配缓存和辅助对象.
        if (!glCreateVertexArrays || !glDeleteVertexArrays || !glBindVertexArray || !glVertexArrayAttribFormat ||
            !glVertexArrayAttribIFormat || !glVertexArrayAttribBinding || !glEnableVertexArrayAttrib ||
            !glDisableVertexArrayAttrib || !glVertexArrayVertexBuffer || !glVertexArrayBindingDivisor ||
            !glVertexArrayElementBuffer || !glUseProgram || !glPointSize || !glLineWidth || !glBindFramebuffer ||
            !glBindBufferRange || !glBindBufferBase || !glBindTextureUnit || !glBindSampler || !glViewportIndexedf ||
            !glDepthRangeIndexed || !glScissorIndexed || !glEnablei || !glDisablei || !glEnable || !glDisable ||
            !glDepthMask || !glDepthFunc || !glStencilMaskSeparate || !glStencilFuncSeparate || !glStencilOpSeparate ||
            !glBlendFuncSeparatei || !glBlendEquationSeparatei || !glBlendColor || !glColorMaski || !glPolygonMode ||
            !glFrontFace || !glCullFace || !glProvokingVertex || !glPolygonOffsetClamp || !glClearNamedFramebufferfv ||
            !glClearNamedFramebufferiv || !glClearNamedFramebufferuiv || !glClearNamedFramebufferfi ||
            !glCopyImageSubData || !glBlitNamedFramebuffer || !glNamedFramebufferDrawBuffer ||
            !glDrawArraysInstancedBaseInstance || !glDrawElementsInstancedBaseVertexBaseInstance ||
            !glDrawElementsIndirect || !glMultiDrawElementsIndirect || !glMemoryBarrierByRegion)
        {
            device.Report("CreateDevice.Graphics", "Unsupported: required graphics execution entry missing");
            return false;
        }
        const auto &caps = device.GetCaps().shader;
        // 先完成可能分配的 CPU 缓存, 再取得原生辅助对象; 每类缓存均受初始化查询的 limits 约束.
        m_UniformRanges.resize(std::size_t(caps.uniformBindings));
        m_StorageRanges.resize(std::size_t(caps.storageBindings));
        m_TextureUnits.resize(std::size_t(caps.combinedTextureUnits));
        m_Images.resize(std::size_t(caps.imageUnits));
        glCreateVertexArrays(1, &m_VertexArray);
        glCreateFramebuffers(1, &m_ClearFramebuffer);
        glCreateFramebuffers(1, &m_ReadTransferFramebuffer);
        return device.CheckNative("CreateDevice.Graphics") && m_VertexArray != 0 && m_ClearFramebuffer != 0 &&
               m_ReadTransferFramebuffer != 0;
    }

    void ContextState::Shutdown(OpenGLDevice &device, bool nativeAvailable) noexcept
    {
        if (nativeAvailable)
        {
            ReleaseBindings();
            nativeAvailable = device.CheckNative("Shutdown.GraphicsBindings");
        }
        if (nativeAvailable && m_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &m_VertexArray);
            nativeAvailable = device.CheckNative("Shutdown.VertexArray");
        }
        if (nativeAvailable)
        {
            const GLuint framebuffers[] = {m_ClearFramebuffer, m_ReadTransferFramebuffer};
            glDeleteFramebuffers(2, framebuffers);
            static_cast<void>(device.CheckNative("Shutdown.TransferFramebuffers"));
        }
        m_VertexArray = 0;
        m_ClearFramebuffer = 0;
        m_ReadTransferFramebuffer = 0;
        Invalidate();
    }

    void ContextState::Invalidate() noexcept
    {
        // 只撤销已知性, 保留名称以便真正删除前仍能解除原生引用; 不查询或保存外部 GL 状态.
        // 累计值跨缓存失效保留, 下一次实际重绑由各 Apply 入口计入.
        m_Pack.known = m_Unpack.known = false;
        m_IndirectKnown = false;
        m_QueryReadKnown = false;
        m_SeamlessCubeKnown = false;
        m_ProgramKnown = m_FramebufferKnown = m_InputKnown = m_IndexKnown = m_VertexArrayKnown = false;
        m_RenderKnown = m_ViewportKnown = m_ScissorKnown = m_SrgbKnown = false;
        for (auto &range : m_UniformRanges)
        {
            range.known = false;
        }
        for (auto &range : m_StorageRanges)
        {
            range.known = false;
        }
        for (auto &image : m_Images)
        {
            image.known = false;
        }
        for (auto &unit : m_TextureUnits)
        {
            unit.known = false;
        }
        for (auto &binding : m_VertexBindings)
        {
            binding.known = false;
        }
    }

    void ContextState::ReleaseBindings() noexcept
    {
        // 唯一 VAO 不跨帧扣留已退休 Buffer. 原生删除发生前先解除 native 对象之间的引用.
        if (m_VertexArray != 0)
        {
            // 单一 VAO 的 element 解绑走绑定入口, 避开本机 DSA 对合法 buffer=0 的拒绝.
            glBindVertexArray(m_VertexArray);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            m_BindingCount += 2;
            for (GLuint index = 0; index < m_VertexBindings.size(); ++index)
            {
                glVertexArrayVertexBuffer(m_VertexArray, index, 0, 0, 0);
                ++m_BindingCount;
                m_VertexBindings[index].name = 0;
            }
        }
        for (GLuint index = 0; index < m_UniformRanges.size(); ++index)
        {
            if (m_UniformRanges[index].name != 0)
            {
                glBindBufferBase(GL_UNIFORM_BUFFER, index, 0);
                ++m_BindingCount;
            }
            m_UniformRanges[index].name = 0;
        }
        for (GLuint index = 0; index < m_StorageRanges.size(); ++index)
        {
            if (m_StorageRanges[index].name != 0)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, 0);
                ++m_BindingCount;
            }
            m_StorageRanges[index].name = 0;
        }
        for (GLuint index = 0; index < m_TextureUnits.size(); ++index)
        {
            if (m_TextureUnits[index].texture != 0)
            {
                glBindTextureUnit(index, 0);
                ++m_BindingCount;
            }
            if (m_TextureUnits[index].sampler != 0)
            {
                glBindSampler(index, 0);
                ++m_BindingCount;
            }
            m_TextureUnits[index] = {};
        }
        // image 单元也持有原生对象绑定. 删除或关闭前解除引用, 下次使用由 Pipeline 计划重新建立.
        for (GLuint index = 0; index < m_Images.size(); ++index)
        {
            if (m_Images[index].name != 0)
            {
                glBindImageTexture(index, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
                ++m_BindingCount;
                m_Images[index] = {};
            }
        }
        if (m_IndirectBuffer != 0)
        {
            // 参数绑定属于 context. 单独解除它, 才能让已退休的参数缓冲脱离此绑定点.
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            ++m_BindingCount;
            m_IndirectBuffer = 0;
        }
        if (m_Pack.buffer != 0)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            ++m_BindingCount;
            m_Pack.buffer = 0;
        }
        if (m_Unpack.buffer != 0)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            ++m_BindingCount;
            m_Unpack.buffer = 0;
        }
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_BindingCount += 2;
        m_Program = m_Framebuffer = m_IndexBuffer = 0;
        Invalidate();
    }

    void ContextState::ApplyQueryRead() noexcept
    {
        // 不查询/恢复外部绑定, 只在缓存未知时建立客户端输出指针所需状态, 不扰动其它图形缓存.
        if (!m_QueryReadKnown)
        {
            glBindBuffer(GL_QUERY_BUFFER, 0);
            ++m_BindingCount;
            m_QueryReadKnown = true;
        }
    }

    void ContextState::ApplyProgram(GLuint program, bool pointSize) noexcept
    {
        if (!m_ProgramKnown || m_Program != program)
        {
            glUseProgram(program);
            ++m_BindingCount;
        }
        if (!m_ProgramKnown || m_PointSize != pointSize)
        {
            if (pointSize)
            {
                glEnable(GL_PROGRAM_POINT_SIZE);
            }
            else
            {
                glDisable(GL_PROGRAM_POINT_SIZE);
                glPointSize(1.f);
            }
        }
        m_Program = program;
        m_PointSize = pointSize;
        m_ProgramKnown = true;
    }

    void ContextState::ApplyFramebuffer(GLuint framebuffer) noexcept
    {
        if (!m_FramebufferKnown || m_Framebuffer != framebuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            ++m_BindingCount;
        }
        m_Framebuffer = framebuffer;
        m_FramebufferKnown = true;
    }

    void ContextState::ApplyBufferRange(bool storage, GLuint unit, GLuint name, GLintptr offset,
                                        GLsizeiptr size) noexcept
    {
        auto &current = (storage ? m_StorageRanges : m_UniformRanges)[unit];
        if (!current.known || current.name != name || current.offset != offset || current.size != size)
        {
            glBindBufferRange(storage ? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER, unit, name, offset, size);
            ++m_BindingCount;
        }
        current = {name, offset, size, true};
    }

    void ContextState::ApplyImage(GLuint unit, GLuint name, bool layered, GLenum access, GLenum format) noexcept
    {
        auto &state = m_Images[unit];
        if (!state.known || state.name != name || state.layered != layered ||
            state.access != access || state.format != format)
        {
            glBindImageTexture(unit, name, 0, layered ? GL_TRUE : GL_FALSE, 0, access, format);
            ++m_BindingCount;
            state = {name, layered, access, format, true};
        }
    }

    void ContextState::ApplyTexture(GLuint unit, GLuint texture, GLuint sampler, TextureDimension dimension) noexcept
    {
        // 只在实际 cube 采样时建立跨面过滤. 外部互操作后随其余缓存重新应用.
        if (dimension == TextureDimension::TextureCube && !m_SeamlessCubeKnown)
        {
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
            m_SeamlessCubeKnown = true;
        }
        auto &current = m_TextureUnits[unit];
        if (!current.known || current.texture != texture)
        {
            glBindTextureUnit(unit, texture);
            ++m_BindingCount;
        }
        if (!current.known || current.sampler != sampler)
        {
            glBindSampler(unit, sampler);
            ++m_BindingCount;
        }
        current = {texture, sampler, true};
    }

    void ContextState::ApplyInput(const OpenGLInputLayout *layout) noexcept
    {
        // 包含禁用旧 location 的完整差分, 不能只更新新布局包含的属性而留下旧的启用位.
        const auto inputs = layout ? std::span<const VertexInput>(layout->Inputs()) : std::span<const VertexInput>();
        if (!m_VertexArrayKnown)
        {
            glBindVertexArray(m_VertexArray);
            ++m_BindingCount;
        }
        m_VertexArrayKnown = true;
        for (GLuint location = 0; location < m_Attributes.size(); ++location)
        {
            const auto found = std::ranges::find(inputs, location, &VertexInput::location);
            AttributeState next;
            if (found != inputs.end())
            {
                next = {found->bufferIndex,       found->offset,     found->scalar, found->storage,
                        GLint(found->components), found->normalized, true};
            }
            if (!m_InputKnown || next != m_Attributes[location])
            {
                if (next.enabled)
                {
                    glEnableVertexArrayAttrib(m_VertexArray, location);
                    glVertexArrayAttribBinding(m_VertexArray, location, next.binding);
                    if (next.scalar == GL_FLOAT)
                    {
                        // normalized 与浮点转换只用于 float Shader 输入, 整数输入保持 IFormat 的整数取数.
                        glVertexArrayAttribFormat(m_VertexArray, location, next.count, next.storage,
                                                  GLboolean(next.normalized), next.offset);
                    }
                    else
                    {
                        glVertexArrayAttribIFormat(m_VertexArray, location, next.count, next.storage, next.offset);
                    }
                }
                else
                {
                    glDisableVertexArrayAttrib(m_VertexArray, location);
                }
                m_Attributes[location] = next;
            }
        }
        m_InputKnown = true;
    }

    void ContextState::ApplyVertexBuffer(GLuint slot, GLuint name, GLintptr offset, GLsizei stride,
                                         GLuint divisor) noexcept
    {
        auto &current = m_VertexBindings[slot];
        if (!current.known || current.name != name || current.offset != offset || current.stride != stride)
        {
            glVertexArrayVertexBuffer(m_VertexArray, slot, name, offset, stride);
            ++m_BindingCount;
        }
        if (!current.known || current.divisor != divisor)
        {
            glVertexArrayBindingDivisor(m_VertexArray, slot, divisor);
        }
        current = {name, offset, stride, divisor, true};
    }

    void ContextState::ApplyIndexBuffer(GLuint name) noexcept
    {
        if (!m_IndexKnown || m_IndexBuffer != name)
        {
            ++m_BindingCount;
            if (name != 0)
            {
                glVertexArrayElementBuffer(m_VertexArray, name);
            }
            else
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }
        m_IndexBuffer = name;
        m_IndexKnown = true;
    }

    void ContextState::ApplyIndirectBuffer(GLuint name) noexcept
    {
        if (!m_IndirectKnown || m_IndirectBuffer != name)
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, name);
            ++m_BindingCount;
            m_IndirectBuffer = name;
            m_IndirectKnown = true;
        }
    }

    void ContextState::ApplyUnpack(GLuint buffer, GLint alignment, GLint rowLength) noexcept
    {
        ApplyPixelTransfer(false, buffer, alignment, rowLength);
    }

    void ContextState::ApplyPack(GLuint buffer, GLint alignment, GLint rowLength) noexcept
    {
        ApplyPixelTransfer(true, buffer, alignment, rowLength);
    }

    // pack 和 unpack 分别缓存. 失效后重建全部像素存储参数, 已知状态间只更新 PBO, 对齐或行长.
    void ContextState::ApplyPixelTransfer(bool pack, GLuint buffer, GLint alignment, GLint rowLength) noexcept
    {
        auto &state = pack ? m_Pack : m_Unpack;
        if (!state.known || state.buffer != buffer)
        {
            glBindBuffer(pack ? GL_PIXEL_PACK_BUFFER : GL_PIXEL_UNPACK_BUFFER, buffer);
            ++m_BindingCount;
        }
        if (!state.known)
        {
            // 调用者通过 offset 定位区域. 压缩入口用紧密完整块行, 不继承外部跳过或块重排参数.
            glPixelStorei(pack ? GL_PACK_IMAGE_HEIGHT : GL_UNPACK_IMAGE_HEIGHT, 0);
            glPixelStorei(pack ? GL_PACK_SKIP_PIXELS : GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(pack ? GL_PACK_SKIP_ROWS : GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(pack ? GL_PACK_SKIP_IMAGES : GL_UNPACK_SKIP_IMAGES, 0);
            glPixelStorei(pack ? GL_PACK_SWAP_BYTES : GL_UNPACK_SWAP_BYTES, GL_FALSE);
            glPixelStorei(pack ? GL_PACK_LSB_FIRST : GL_UNPACK_LSB_FIRST, GL_FALSE);
            glPixelStorei(pack ? GL_PACK_COMPRESSED_BLOCK_WIDTH : GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
            glPixelStorei(pack ? GL_PACK_COMPRESSED_BLOCK_HEIGHT : GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
            glPixelStorei(pack ? GL_PACK_COMPRESSED_BLOCK_DEPTH : GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0);
            glPixelStorei(pack ? GL_PACK_COMPRESSED_BLOCK_SIZE : GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
        }
        if (!state.known || state.alignment != alignment)
        {
            glPixelStorei(pack ? GL_PACK_ALIGNMENT : GL_UNPACK_ALIGNMENT, alignment);
        }
        if (!state.known || state.rowLength != rowLength)
        {
            glPixelStorei(pack ? GL_PACK_ROW_LENGTH : GL_UNPACK_ROW_LENGTH, rowLength);
        }
        state = {buffer, alignment, rowLength, true};
    }
}
