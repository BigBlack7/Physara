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
    };

    // setup 发布完整覆盖的新版本, execute 到首次物化后才取得实际纹理.
    GraphResult<GraphTextureWrite> AddTextureUpload(RenderGraph &graph, TextureId target,
        std::span<const std::byte> pixels, std::size_t rowPitch, std::vector<GraphUpload> &uploads)
    {
        TextureId written;
        auto pass = graph.AddPass<TextureUploadData>("Texture upload", [&](PassBuilder &builder, auto &data)
        {
            written = builder.Write(target, GraphUsage::CopyDestination, {.coverage = WriteCoverage::Full});
            data = {written, pixels, rowPitch};
            return written.IsValid();
        }, +[](PassResources &resources, const TextureUploadData &data, RHI::CommandList &commands) noexcept
            -> GraphFailure
        {
            if (auto *texture = resources.Resolve(data.texture, GraphUsage::CopyDestination))
            {
                commands.WriteTexture(texture, 0, 0, data.pixels.data(), data.rowPitch);
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
    };

    // 声明与执行使用相同字节区间, 使后续顶点, 索引和常量读取有可追溯的初始化来源.
    GraphResult<BufferId> AddBufferUpload(RenderGraph &graph, BufferId target,
        std::span<const std::byte> bytes, std::vector<GraphUpload> &uploads)
    {
        BufferId written;
        auto pass = graph.AddPass<BufferUploadData>("UI buffer upload", [&](PassBuilder &builder, auto &data)
        {
            written = builder.Write(target, GraphUsage::CopyDestination, RHI::BufferRange(0, bytes.size()));
            data = {written, bytes};
            return written.IsValid();
        }, +[](PassResources &resources, const BufferUploadData &data, RHI::CommandList &commands) noexcept
            -> GraphFailure
        {
            if (auto *buffer = resources.Resolve(data.buffer, GraphUsage::CopyDestination,
                                                RHI::BufferRange(0, data.bytes.size())))
            {
                commands.WriteBuffer(buffer, data.bytes.data(), data.bytes.size());
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

    // 输出对象可由调用方转为强引用, GraphId 仍只用于匹配这次执行的确切版本.
    RHI::Texture *FindTextureOutput(const GraphExecutionOutputList &outputs, TextureId id) noexcept
    {
        for (const auto &item : outputs)
        {
            if (const auto *texture = std::get_if<GraphTextureOutput>(&item);
                texture && texture->declaration.resource == id)
            {
                return texture->object.Get();
            }
        }
        return nullptr;
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
