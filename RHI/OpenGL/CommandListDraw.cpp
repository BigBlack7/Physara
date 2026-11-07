#include "CommandList.hpp"

/// @file
/// @brief 对本次直接 Draw 参数及可知数据范围做边界检查, 即时发出原生命令, 不保存重放记录.

#include <algorithm>
#include <limits>

#include "InputLayout.hpp"
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    void OpenGLCommandList::Draw(const DrawArguments &args)
    {
        DrawDirect(args, false);
    }

    void OpenGLCommandList::DrawIndexed(const DrawArguments &args)
    {
        DrawDirect(args, true);
    }

    void OpenGLCommandList::DrawDirect(const DrawArguments &args, bool indexed)
    {
        // 本次 count/offset 校验先于用途转换和 native 应用, 合法空 Draw 在准备工作前返回.
        const auto *operation = indexed ? "DrawIndexed" : "Draw";
        auto *packet = Recording(operation);
        if (!packet)
        {
            return;
        }
        const auto &state = packet->graphics;
        const auto *pipeline = static_cast<OpenGLGraphicsPipeline *>(state.pipeline);
        if (!packet->graphicsSet || !packet->viewportSet ||
            (pipeline->GetDesc().renderState.rasterState.scissorEnable && !packet->scissorSet) ||
            (indexed && !state.indexBuffer.buffer))
        {
            m_Device->Report(operation, "InvalidState: graphics, viewport/scissor or index binding missing");
            return;
        }
        if (args.vertexCount > std::uint32_t(std::numeric_limits<GLsizei>::max()) ||
            args.instanceCount > std::uint32_t(std::numeric_limits<GLsizei>::max()) ||
            args.startVertexLocation > std::uint32_t(std::numeric_limits<GLint>::max()))
        {
            m_Device->Report(operation, "InvalidArgument: count/first vertex cannot be represented natively");
            return;
        }
        if (args.vertexCount == 0 || args.instanceCount == 0)
        {
            return;
        }
        std::uint64_t indexOffset = 0;
        if (indexed)
        {
            // 公共 offset 是字节, first index 是元素数; 不把 baseVertex 加进索引地址.
            const auto stride = state.indexBuffer.format == Format::R16_UINT ? 2u : 4u;
            indexOffset = std::uint64_t(state.indexBuffer.offset) + std::uint64_t(args.startIndexLocation) * stride;
            const auto size = std::uint64_t(args.vertexCount) * stride;
            const auto capacity = state.indexBuffer.buffer->GetDesc().byteSize;
            if (indexOffset > capacity || size > capacity - indexOffset)
            {
                m_Device->Report(operation, "InvalidArgument: index byte interval exceeds buffer");
                return;
            }
        }
        if (const auto *input = static_cast<OpenGLInputLayout *>(pipeline->GetDesc().inputLayout.Get()))
        {
            for (const auto &attribute : input->Inputs())
            {
                const auto found =
                    std::ranges::find(state.vertexBuffers, attribute.bufferIndex, &VertexBufferBinding::slot);
                // indexed 内容由调用方/资产边界保证; 不为求最大索引增加 GPU 同步读回.
                // 已检查的 count/stride 上限保证下面的宽算术可表示, 实例属性 divisor 固定为 1.
                const auto last = attribute.instanced
                                      ? std::uint64_t(args.startInstanceLocation) + args.instanceCount - 1
                                      : std::uint64_t(args.startVertexLocation) + (indexed ? 0 : args.vertexCount - 1);
                const auto begin = std::uint64_t(found->offset) + attribute.offset + last * attribute.stride;
                const auto capacity = found->buffer->GetDesc().byteSize;
                if (begin > capacity || attribute.byteSize > capacity - begin)
                {
                    m_Device->Report(operation, "InvalidArgument: vertex/instance byte interval exceeds buffer");
                    return;
                }
            }
        }
        // 上传/clear/copy 可能改变用途或 native 状态, 即使没有再次 SetGraphicsState 也要重新满足它们.
        if (!GraphicsUses(state, *pipeline, operation) || !m_Device->BeginNative(operation))
        {
            return;
        }
        ApplyGraphics(state, *pipeline);
        constexpr GLenum Topologies[] = {
            GL_POINTS,         GL_LINES,        GL_LINE_STRIP,          GL_TRIANGLES,
            GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES_ADJACENCY, GL_TRIANGLE_STRIP_ADJACENCY};
        const auto topology = Topologies[unsigned(pipeline->GetDesc().primType)];
        // 首个可能发出的 Draw 前登记工作; 后续诊断失败也不能撤销前缀或提前释放其依赖.
        packet->issuedWork = true;
        if (indexed)
        {
            glDrawElementsInstancedBaseVertexBaseInstance(
                topology, GLsizei(args.vertexCount),
                state.indexBuffer.format == Format::R16_UINT ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                reinterpret_cast<const void *>(std::uintptr_t(indexOffset)), GLsizei(args.instanceCount),
                GLint(args.startVertexLocation), args.startInstanceLocation);
        }
        else
        {
            glDrawArraysInstancedBaseInstance(topology, GLint(args.startVertexLocation), GLsizei(args.vertexCount),
                                              GLsizei(args.instanceCount), args.startInstanceLocation);
        }
        if (!m_Device->CheckNative(operation))
        {
            packet->invalid = true;
            m_Device->m_ContextState.Invalidate();
        }
    }
} // namespace Physara::RHI::OpenGL
