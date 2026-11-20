#include "CommandList.hpp"

/// @file
/// @brief 校验直接或间接 Draw 的可知范围, 共用图形准备后即时发出原生命令.

#include <algorithm>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "InputLayout.hpp"
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    static constexpr GLenum Topologies[] = {
        GL_POINTS,         GL_LINES,        GL_LINE_STRIP,          GL_TRIANGLES,
        GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES_ADJACENCY, GL_TRIANGLE_STRIP_ADJACENCY};

    // 公开记录即 GL 的紧排 indexed 参数 ABI. CPU 调用方以字节定位记录, GPU 解释五个字段.
    static_assert(sizeof(DrawIndexedIndirectArguments) == 20 &&
                  offsetof(DrawIndexedIndirectArguments, indexCount) == 0 &&
                  offsetof(DrawIndexedIndirectArguments, instanceCount) == 4 &&
                  offsetof(DrawIndexedIndirectArguments, startIndexLocation) == 8 &&
                  offsetof(DrawIndexedIndirectArguments, baseVertexLocation) == 12 &&
                  offsetof(DrawIndexedIndirectArguments, startInstanceLocation) == 16 &&
                  std::is_same_v<decltype(DrawIndexedIndirectArguments::baseVertexLocation), std::int32_t>);

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
        const auto *pipeline = DrawPipeline(indexed, operation);
        if (!pipeline)
        {
            return;
        }
        const auto &state = packet->graphics;
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
            if (!CheckGraphBufferUse(*static_cast<OpenGLBuffer *>(state.indexBuffer.buffer),
                                     ResourceStates::IndexBuffer, operation, BufferRange{indexOffset, size}))
            {
                return;
            }
        }
        if (const auto *input = static_cast<OpenGLInputLayout *>(pipeline->GetDesc().inputLayout.Get()))
        {
            for (const auto &attribute : input->Inputs())
            {
                const auto found =
                    std::ranges::find(state.vertexBuffers, attribute.bufferIndex, &VertexBufferBinding::slot);
                // 非索引顶点和实例从命令参数得到首末元素, 实例 divisor 固定为 1.
                // 已校验的 count 和 stride 上限使乘加在宽字节类型内可表示.
                // Indexed 的逐顶点取值由索引数据决定, 这里只核对 baseVertex 的容量, 具体索引值由数据提供方保证.
                const auto first = attribute.instanced ? args.startInstanceLocation : args.startVertexLocation;
                const bool knownRange = attribute.instanced || !indexed;
                const auto count = !knownRange ? 1u : (attribute.instanced ? args.instanceCount : args.vertexCount);
                const auto firstByte = attribute.offset + std::uint64_t(first) * attribute.stride;
                const auto lastByte = firstByte + std::uint64_t(count - 1) * attribute.stride;
                const auto capacity = found->buffer->GetDesc().byteSize;
                const auto available = capacity - found->offset;
                if (lastByte > available || attribute.byteSize > available - lastByte)
                {
                    m_Device->Report(operation, "InvalidArgument: vertex/instance byte interval exceeds buffer");
                    return;
                }
                const BufferRange covered{found->offset + firstByte, lastByte - firstByte + attribute.byteSize};
                if (!CheckGraphBufferUse(*static_cast<OpenGLBuffer *>(found->buffer),
                                         ResourceStates::VertexBuffer, operation, covered))
                {
                    return;
                }
            }
        }
        if (!PrepareDraw(*pipeline, operation))
        {
            return;
        }
        const auto topology = Topologies[unsigned(pipeline->GetDesc().primType)];
        // 首个可能发出的 Draw 前登记工作; 后续诊断失败也不能撤销前缀或提前释放其依赖.
        packet->issuedWork = true;
        // instanced Draw 只发出一次 GL 调用. 该计数与绘制了多少实例或前面设置了几次状态无关.
        ++m_Device->m_CommandCounts.draws;
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
        FinishDraw(*pipeline, operation);
    }

    void OpenGLCommandList::DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        constexpr auto operation = "DrawIndexedIndirect";
        auto *packet = Recording(operation);
        if (!packet)
        {
            return;
        }
        const auto *pipeline = DrawPipeline(true, operation);
        if (!pipeline)
        {
            return;
        }
        const auto &state = packet->graphics;
        auto *parameters = static_cast<OpenGLBuffer *>(state.indirectParams);
        if (!parameters)
        {
            m_Device->Report(operation, "InvalidState: indirect argument binding missing");
            return;
        }
        // GL 的 firstIndex 从 element Buffer 起点计, 无法叠加独立的索引绑定偏移.
        if (state.indexBuffer.offset != 0)
        {
            m_Device->Report(operation, "Unsupported: indexed indirect requires zero index binding byte offset");
            return;
        }
        const auto bytes = std::uint64_t(drawCount) * sizeof(DrawIndexedIndirectArguments);
        const auto capacity = parameters->GetDesc().byteSize;
        if (offsetBytes % sizeof(std::uint32_t) != 0 || drawCount > std::uint32_t(std::numeric_limits<GLsizei>::max()) ||
            offsetBytes > capacity || bytes > capacity - offsetBytes)
        {
            m_Device->Report(operation, "InvalidArgument: indirect count, alignment or byte interval",
                             parameters->GetDesc().debugName.c_str());
            return;
        }
        if (drawCount == 0)
        {
            return;
        }
        // 参数内容对 CPU 不透明. GraphManaged 仍核对本次可知的完整参数区间, 几何范围由生产者保证.
        if (!UseBuffer(*parameters, ResourceStates::IndirectArgument, operation, BufferRange{offsetBytes, bytes}) ||
            !PrepareDraw(*pipeline, operation))
        {
            return;
        }
        m_Device->m_ContextState.ApplyIndirectBuffer(parameters->Name());
        const auto topology = Topologies[unsigned(pipeline->GetDesc().primType)];
        const auto indexType = state.indexBuffer.format == Format::R16_UINT ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        // 绑定参数 Buffer 后, GL 的指针形参解释为字节偏移. MDI 的 stride=0 选择连续的 20 字节记录.
        const auto *offset = reinterpret_cast<const void *>(std::uintptr_t(offsetBytes));
        packet->issuedWork = true;
        ++m_Device->m_CommandCounts.draws;
        if (drawCount == 1)
        {
            glDrawElementsIndirect(topology, indexType, offset);
        }
        else
        {
            glMultiDrawElementsIndirect(topology, indexType, offset, GLsizei(drawCount), 0);
        }
        FinishDraw(*pipeline, operation);
    }

    const OpenGLGraphicsPipeline *OpenGLCommandList::DrawPipeline(bool indexed, const char *operation)
    {
        const auto &packet = *m_Packet;
        const auto &state = packet.graphics;
        const auto *pipeline = static_cast<OpenGLGraphicsPipeline *>(state.pipeline);
        if (!packet.graphicsSet || !packet.viewportSet ||
            (pipeline->GetDesc().renderState.rasterState.scissorEnable && !packet.scissorSet) ||
            (indexed && !state.indexBuffer.buffer))
        {
            m_Device->Report(operation, "InvalidState: graphics, viewport/scissor or index binding missing");
            return nullptr;
        }
        return pipeline;
    }

    bool OpenGLCommandList::PrepareDraw(const OpenGLGraphicsPipeline &pipeline, const char *operation)
    {
        const auto &state = m_Packet->graphics;
        // 上传, clear 或 copy 可改变用途及缓存. 同一图形状态也需要选择最近写好的常量版本.
        if (!CheckDrawConstants(state, pipeline, operation) || !GraphicsUses(state, pipeline, operation) ||
            !m_Device->BeginNative(operation) || !PrepareDepthSampling(state, pipeline))
        {
            return false;
        }
        ApplyGraphics(state, pipeline);
        ApplyDrawConstants(state, pipeline);
        return true;
    }

    void OpenGLCommandList::FinishDraw(const OpenGLGraphicsPipeline &pipeline, const char *operation)
    {
        if (!m_Device->CheckNative(operation))
        {
            m_Packet->invalid = true;
            m_Device->m_ContextState.Invalidate();
            return;
        }
        const auto &attachment = m_Packet->graphics.framebuffer->GetDesc().depthAttachment;
        const auto &depth = pipeline.GetDesc().renderState.depthStencilState;
        // Draw 成功且允许深度写时, 按底层 mip 标记可能修改的深度内容. 同格式采样读取深度分量.
        if (attachment.texture && depth.depthTestEnable && depth.depthWriteEnable)
        {
            RecordDepthWrite(*static_cast<OpenGLTexture *>(attachment.texture), attachment.subresources);
        }
    }
} // namespace Physara::RHI::OpenGL
