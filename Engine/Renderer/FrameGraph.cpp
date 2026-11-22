#include "FrameGraph.hpp"

#include <algorithm>

/// @file
/// @brief 把产品拥有的上传字节包装为独立图 Pass, 并从编译计划提取实际写入顺序.
/// 回调只发出声明对应的 Write; 查询失败由 PassResources 记录, RHI 错误由执行器统一观察.

namespace Physara::Renderer
{
    /// @brief 拥有版本和范围, 借用已复制到帧槽的上传字节.
    struct TextureUploadData
    {
        TextureId texture;
        std::span<const std::byte> pixels;
        std::size_t rowPitch = 0;
        TextureRange range;
        bool *recorded = nullptr;
    };

    // 每次完整定义指定 mip/layer, 其它子资源由各自生产者回溯. 执行沿用同一范围及行距.
    GraphResult<GraphTextureWrite> AddTextureUpload(RenderGraph &graph, TextureId target,
        std::span<const std::byte> pixels, std::size_t rowPitch, std::vector<GraphUpload> &uploads, std::uint32_t mip,
        std::uint32_t layer, bool *recorded)
    {
        const TextureRange range{.firstMip = mip, .firstLayer = layer};
        TextureId written;
        auto pass = graph.AddPass<TextureUploadData>("Texture upload", [&](PassBuilder &builder, auto &data)
        {
            written = builder.Write(target, GraphUsage::CopyDestination, {.coverage = WriteCoverage::Full}, range);
            data = {written, pixels, rowPitch, range, recorded};
            return written.IsValid();
        }, +[](PassResources &resources, const TextureUploadData &data, RHI::CommandList &commands) noexcept
            -> GraphFailure
        {
            if (auto *texture = resources.Resolve(data.texture, GraphUsage::CopyDestination, data.range))
            {
                if (data.recorded)
                {
                    // 同代可能已交给原生写入, 后续未获接受时只能用独立存储重试.
                    *data.recorded = true;
                }
                commands.WriteTexture(texture, data.range.firstLayer, data.range.firstMip, data.pixels.data(),
                                      data.rowPitch);
            }
            return std::nullopt;
        });
        if (auto *error = std::get_if<GraphError>(&pass))
        {
            return *error;
        }
        const auto id = std::get<PassId>(pass);
        uploads.push_back({id, pixels.size(), true});
        return GraphTextureWrite{written, id};
    }

    /// @brief Buffer 的精确写版本及借用字节. 不以物理容量代替本次写入长度.
    struct BufferUploadData
    {
        BufferId buffer;
        std::span<const std::byte> bytes;
        RHI::BufferRange range;
        bool *recorded;
    };

    // 将物理页内的绝对区间同时交给声明和延迟回调, Draw 可沿写版本追溯实际被初始化的字节.
    GraphResult<BufferId> AddBufferUpload(RenderGraph &graph, BufferId target,
        std::span<const std::byte> bytes, std::vector<GraphUpload> &uploads, std::uint64_t offset, bool *recorded)
    {
        BufferId written;
        const RHI::BufferRange range(offset, bytes.size());
        auto pass = graph.AddPass<BufferUploadData>("Buffer upload", [&](PassBuilder &builder, auto &data)
        {
            written = builder.Write(target, GraphUsage::CopyDestination, range);
            data = {written, bytes, range, recorded};
            return written.IsValid();
        }, +[](PassResources &resources, const BufferUploadData &data, RHI::CommandList &commands) noexcept
            -> GraphFailure
        {
            if (auto *buffer = resources.Resolve(data.buffer, GraphUsage::CopyDestination, data.range))
            {
                if (data.recorded)
                {
                    *data.recorded = true;
                }
                commands.WriteBuffer(buffer, data.bytes.data(), data.bytes.size(), data.range.byteOffset);
            }
            return std::nullopt;
        });
        if (auto *error = std::get_if<GraphError>(&pass))
        {
            return *error;
        }
        uploads.push_back({std::get<PassId>(pass), bytes.size(), false});
        return written;
    }

    // 同索引的旧图 Pass 不构成本轮执行证据, 先匹配图代再检查存活集合.
    bool IsLivePass(const CompiledGraphPlan &plan, PassId pass) noexcept
    {
        return pass.generation == plan.graphGeneration &&
               std::ranges::any_of(plan.passes, [&](const auto &item) { return item.id == pass; });
    }

    // 结果借用自输出表. 根据消费者用途匹配版本或完整交接合同.
    template <typename Matches>
    static const GraphTextureOutput *FindMatchingTextureExport(const GraphExecutionOutputList &outputs,
                                                                Matches matches) noexcept
    {
        for (const auto &item : outputs)
        {
            if (const auto *texture = std::get_if<GraphTextureOutput>(&item);
                texture && matches(texture->declaration))
            {
                return texture;
            }
        }
        return nullptr;
    }

    const GraphTextureOutput *FindTextureExport(const GraphExecutionOutputList &outputs, TextureId id) noexcept
    {
        return FindMatchingTextureExport(outputs, [id](const TextureExport &actual) noexcept
        {
            return actual.resource == id;
        });
    }

    const GraphTextureOutput *FindTextureExport(const GraphExecutionOutputList &outputs,
                                                const TextureExport &expected) noexcept
    {
        return FindMatchingTextureExport(outputs, [&expected](const TextureExport &actual) noexcept
        {
            return actual.resource == expected.resource && actual.range == expected.range &&
                actual.usage == expected.usage && actual.ownerWrite == expected.ownerWrite;
        });
    }

    RHI::Texture *FindTextureOutput(const GraphExecutionOutputList &outputs, TextureId id) noexcept
    {
        const auto *output = FindTextureExport(outputs, id);
        return output ? output->Get() : nullptr;
    }

    // 以拓扑计划为顺序权威. 被裁剪的声明不进入设备准入, 区域复制也不重复计作 CPU 上传.
    GraphResult<GraphUploadBudget> CollectGraphUploads(const CompiledGraphPlan &plan,
                                                       std::span<const GraphUpload> uploads)
    {
        GraphUploadBudget budget;
        for (const auto &pass : plan.passes)
        {
            const auto upload = std::ranges::find(uploads, pass.id, &GraphUpload::pass);
            if (upload == uploads.end())
            {
                continue;
            }
            if (upload->texture && !budget.buffers.empty())
            {
                return GraphError{GraphErrorCode::InvalidUsage,
                    Core::BoundedText("Product upload order requires textures before buffers")};
            }
            (upload->texture ? budget.textures : budget.buffers).push_back(upload->bytes);
        }
        return budget;
    }
}
