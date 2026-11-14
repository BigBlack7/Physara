#include "GraphExecutionStates.hpp"

/// @file
/// @brief 检查瞬态图尾冲突, 合并兼容状态, 并通过当前列表登记和提交单个执行阶段.

#include <charconv>
#include <cstdint>

#include "GraphRanges.hpp"

namespace Physara::Renderer::Detail
{
    RHI::ResourceStates OutputState(const TextureExport &output) noexcept
    {
        return output.usage == ExportUsage::ShaderRead ? SampledState(output.range.aspect)
                                                      : RHI::ResourceStates::CopySource;
    }

    RHI::ResourceStates OutputState(const BufferExport &output) noexcept
    {
        return output.usage == ExportUsage::ShaderRead ? RHI::ResourceStates::ShaderResource
                                                      : RHI::ResourceStates::CopySource;
    }

    // 把冲突范围的数值写入有界错误, 预检失败时无需额外字符串存储.
    static void AppendNumber(Core::BoundedText &text, std::uint64_t value) noexcept
    {
        char digits[20];
        const auto result = std::to_chars(digits, digits + sizeof(digits), value);
        text.Append(std::string_view(digits, result.ptr));
    }

    // 仅报告实际相交的追踪单元. 深度和模板在同一 mip/layer 共享一个状态.
    static GraphFailure ExportConflict(const TextureExport &output, const TextureExport &other,
                                       std::string_view name) noexcept
    {
        const auto [mip, mips] = IntersectInterval(
            output.range.firstMip, output.range.mipCount, other.range.firstMip, other.range.mipCount);
        const auto [layer, layers] = IntersectInterval(
            output.range.firstLayer, output.range.layerCount, other.range.firstLayer, other.range.layerCount);
        Core::BoundedText reason("ShaderRead + Readback conflict; mip ");
        AppendNumber(reason, mip);
        reason.Append(" count ");
        AppendNumber(reason, mips);
        reason.Append(", layer ");
        AppendNumber(reason, layer);
        reason.Append(" count ");
        AppendNumber(reason, layers);
        return MakeGraphError(GraphErrorCode::ConflictingExport, "GraphExec.Preflight", {}, name, reason.View(),
                              output.resource.version);
    }

    GraphFailure CheckTransientExports(const RenderGraph &graph, const CompiledGraphPlan &plan) noexcept
    {
        // 导出延寿至同一图尾, 因此不同逻辑导出资源不会共享槽. 导入的出口始终由其 final 覆盖.
        for (std::size_t i = 0; i < plan.exports.size(); ++i)
        {
            const auto &entry = plan.exports[i];
            if (const auto *texture = std::get_if<TextureExport>(&entry))
            {
                const auto &declared = graph.Textures()[texture->resource.identity.index];
                if (declared.imported)
                {
                    continue;
                }
                for (std::size_t j = 0; j < i; ++j)
                {
                    const auto *other = std::get_if<TextureExport>(&plan.exports[j]);
                    if (other && other->resource.identity == texture->resource.identity &&
                        !SupportsCombinedState(OutputState(*other) | OutputState(*texture)) &&
                        SharesTrackedSubresource(other->range, texture->range))
                    {
                        return ExportConflict(*texture, *other, declared.desc.debugName);
                    }
                }
            }
            else
            {
                const auto &buffer = std::get<BufferExport>(entry);
                const auto &declared = graph.Buffers()[buffer.resource.identity.index];
                if (declared.imported)
                {
                    continue;
                }
                for (std::size_t j = 0; j < i; ++j)
                {
                    const auto *other = std::get_if<BufferExport>(&plan.exports[j]);
                    if (other && other->resource.identity == buffer.resource.identity &&
                        !SupportsCombinedState(OutputState(*other) | OutputState(buffer)))
                    {
                        return MakeGraphError(GraphErrorCode::ConflictingExport, "GraphExec.Preflight", {},
                                              declared.desc.debugName, "ShaderRead + Readback conflict on whole Buffer",
                                              buffer.resource.version);
                    }
                }
            }
        }
        return std::nullopt;
    }

    void GraphExecutionStates::Add(RHI::Texture &texture, const TextureRange &range, RHI::ResourceStates state)
    {
        auto &units = m_Textures[&texture];
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                auto &combined = units[{layer, mip}];
                combined = combined | state;
            }
        }
    }

    void GraphExecutionStates::Add(RHI::Buffer &buffer, RHI::ResourceStates state)
    {
        auto &combined = m_Buffers[&buffer];
        combined = combined | state;
    }

    GraphFailure GraphExecutionStates::Commit(RHI::GraphRecording &recording, GraphExecutionDiagnostics diagnostics,
                                               std::string_view stage, std::string_view pass,
                                               std::span<const RHI::GraphTextureUse> textures,
                                               std::span<const RHI::GraphBufferUse> buffers) const
    {
        for (const auto &[texture, units] : m_Textures)
        {
            // 只把连续且状态相同的 mip 一次交给 RHI; 未选中的单元不会被请求.
            for (auto current = units.begin(); current != units.end();)
            {
                const auto [layer, mip] = current->first;
                const auto state = current->second;
                RHI::MipLevel count = 1;
                while (++current != units.end() && current->first == Subresource{layer, mip + count} &&
                       current->second == state)
                {
                    ++count;
                }
                recording.RequestState(*texture, {mip, count, layer, 1}, state);
                if (!diagnostics.IsClear())
                {
                    return MakeGraphError(GraphErrorCode::ExecutionFailed, stage, pass, texture->GetDesc().debugName,
                                          "SetTextureState failed; see the device diagnostic");
                }
            }
        }
        for (const auto &[buffer, state] : m_Buffers)
        {
            recording.RequestState(*buffer, state);
            if (!diagnostics.IsClear())
            {
                return MakeGraphError(GraphErrorCode::ExecutionFailed, stage, pass, buffer->GetDesc().debugName,
                                      "SetBufferState failed; see the device diagnostic");
            }
        }
        recording.CommitUses(textures, buffers);
        if (!diagnostics.IsClear())
        {
            return MakeGraphError(GraphErrorCode::ExecutionFailed, stage, pass, {},
                                  "CommitBarriers failed; see the device diagnostic");
        }
        return std::nullopt;
    }
}
