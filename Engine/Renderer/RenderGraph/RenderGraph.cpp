#include "RenderGraph.hpp"

#include <atomic>
#include <charconv>
#include <cstdint>
#include <stdexcept>

#include "GraphRanges.hpp"

/// @file
/// @brief 分配图代并发布资源, Pass 及导出记录. 在封存后按 PassId 分派拥有型回调.
/// 图同时管理导入引用和专用 arena 中的对象寿命.

namespace Physara::Renderer
{
    // 主图和备选图可能共用帧槽, 因此图代由所有图共用的计数器分配.
    static std::uint64_t AcquireGeneration() noexcept
    {
        static std::atomic<std::uint64_t> last{0};
        auto observed = last.load(std::memory_order_relaxed);
        auto next = observed;
        while (Core::AdvanceGeneration(next))
        {
            // 原子操作只分配唯一代号. 竞争失败后从观察到的新值继续, 不发布图的其它状态.
            if (last.compare_exchange_weak(observed, next, std::memory_order_relaxed))
            {
                return next;
            }
            next = observed;
        }
        return 0;
    }

    RenderGraph::RenderGraph(Core::FrameArena &arena)
        : m_Arena(arena), m_ArenaStart(arena.Used()), m_Generation(AcquireGeneration())
    {
    }

    RenderGraph::~RenderGraph()
    {
        // 先析构所有拥有型对象, 再让专用 arena 重新使用这些字节.
        for (auto it = m_Callbacks.rbegin(); it != m_Callbacks.rend(); ++it)
        {
            it->destroy(it->data);
        }
        m_Arena.Rewind(m_ArenaStart);
    }

    // 先写原因, 即使 Pass 或资源名称过长也保留失败类别.
    GraphError RenderGraph::MakeError(GraphErrorCode code, std::string_view stage, std::string_view pass,
                                      std::string_view resource, std::string_view reason,
                                      std::optional<std::uint32_t> version) noexcept
    {
        Core::BoundedText message(stage);
        message.Append(": ");
        message.Append(reason);
        if (version)
        {
            char digits[10];
            const auto result = std::to_chars(digits, digits + sizeof(digits), *version);
            message.Append(", version=");
            message.Append({digits, static_cast<std::size_t>(result.ptr - digits)});
        }
        if (!pass.empty())
        {
            message.Append(", pass=");
            message.Append(pass);
        }
        if (!resource.empty())
        {
            message.Append(", resource=");
            message.Append(resource);
        }
        return {code, message};
    }

    // 声明只允许发生在可用图的构建阶段, setup 必须使用它自己的 Builder.
    GraphFailure RenderGraph::CheckBuilding(std::string_view stage, std::string_view pass) const noexcept
    {
        if (m_Generation == 0)
        {
            return MakeError(GraphErrorCode::OutOfStorage, stage, pass, {}, "Graph generation space exhausted");
        }
        if (m_Active || m_Sealed || m_Poisoned)
        {
            return MakeError(GraphErrorCode::InvalidPhase, stage, pass, {}, "Graph is not open for declarations");
        }
        return std::nullopt;
    }

    // 公共 ResourceStates 的有效位从 Common 连续到 ConvertCoopVecMatrixOutput.
    // 进出状态必须能提交给通用 tracker, 具体后端的用途限制由编译和执行检查.
    static bool IsExplicitImportState(RHI::ResourceStates state) noexcept
    {
        constexpr auto lastBit = static_cast<std::uint32_t>(RHI::ResourceStates::ConvertCoopVecMatrixOutput);
        constexpr auto validBits = (lastBit << 1u) - 1u;
        return state != RHI::ResourceStates::Unknown && (static_cast<std::uint32_t>(state) & ~validBits) == 0;
    }

    // 先确定完整范围和导入合同, 再查同一公共 RHI 对象是否已有身份.
    template <typename Id, typename Declaration>
    GraphResult<Id> RenderGraph::DeclareResource(std::string_view stage, std::deque<Declaration> &resources,
                                                const typename Declaration::Description &desc,
                                                std::optional<typename Declaration::AccessRange> requested,
                                                const typename Declaration::ResourceHandle &imported,
                                                std::optional<ImportState> state)
    {
        const auto full = Detail::FullRange(desc);
        if (!full)
        {
            return MakeError(GraphErrorCode::InvalidDescription, stage, {}, desc.debugName,
                             "Resource description has no valid range");
        }
        if (requested && !Detail::Contains(*full, *requested))
        {
            return MakeError(GraphErrorCode::InvalidRange, stage, {}, desc.debugName, "Import range exceeds the resource");
        }
        const auto range = requested.value_or(*full);

        if (state)
        {
            if (!IsExplicitImportState(state->entry))
            {
                return MakeError(GraphErrorCode::InvalidDescription, stage, {}, desc.debugName,
                                 "Import entry state is unknown or contains undefined RHI bits");
            }
            if (!IsExplicitImportState(state->final))
            {
                return MakeError(GraphErrorCode::InvalidDescription, stage, {}, desc.debugName,
                                 "Import final state is unknown or contains undefined RHI bits");
            }

            // 公共 RHI 对象是导入键. 返回版本 0, 后续写入不改变外部输入身份.
            for (std::size_t i = 0; i < resources.size(); ++i)
            {
                const auto &resource = resources[i];
                if (resource.imported.Get() != imported.Get())
                {
                    continue;
                }
                if (resource.range != range || resource.importState != state)
                {
                    return MakeError(GraphErrorCode::ConflictingImport, stage, {}, desc.debugName,
                                     "External resource already has a different import declaration");
                }
                return Id{{static_cast<std::uint32_t>(i), m_Generation}, 0};
            }
        }

        if (resources.size() == MaxIndex)
        {
            return MakeError(GraphErrorCode::OutOfStorage, stage, {}, desc.debugName, "Resource index space exhausted");
        }
        const Id id{{static_cast<std::uint32_t>(resources.size()), m_Generation}, 0};
        try
        {
            resources.emplace_back(desc, range, imported, state);
        }
        catch (const std::bad_alloc &)
        {
            return MakeError(GraphErrorCode::OutOfStorage, stage, {}, desc.debugName, "Could not store resource");
        }
        return id;
    }

    GraphResult<TextureId> RenderGraph::CreateTexture(const RHI::TextureDesc &desc)
    {
        if (auto failure = CheckBuilding("CreateTexture", {}))
        {
            return *failure;
        }
        return DeclareResource<TextureId>("CreateTexture", m_Textures, desc, std::nullopt, {}, std::nullopt);
    }

    GraphResult<BufferId> RenderGraph::CreateBuffer(const RHI::BufferDesc &desc)
    {
        if (auto failure = CheckBuilding("CreateBuffer", {}))
        {
            return *failure;
        }
        return DeclareResource<BufferId>("CreateBuffer", m_Buffers, desc, std::nullopt, {}, std::nullopt);
    }

    GraphResult<TextureId> RenderGraph::ImportTexture(const RHI::TextureHandle &texture, ImportState states,
                                                     std::optional<TextureRange> range)
    {
        if (auto failure = CheckBuilding("ImportTexture", {}))
        {
            return *failure;
        }
        if (!texture)
        {
            return MakeError(GraphErrorCode::InvalidResource, "ImportTexture", {}, {}, "Missing external texture");
        }
        return DeclareResource<TextureId>("ImportTexture", m_Textures, texture->GetDesc(), range, texture, states);
    }

    GraphResult<BufferId> RenderGraph::ImportBuffer(const RHI::BufferHandle &buffer, ImportState states,
                                                   std::optional<RHI::BufferRange> range)
    {
        if (auto failure = CheckBuilding("ImportBuffer", {}))
        {
            return *failure;
        }
        if (!buffer)
        {
            return MakeError(GraphErrorCode::InvalidResource, "ImportBuffer", {}, {}, "Missing external buffer");
        }
        return DeclareResource<BufferId>("ImportBuffer", m_Buffers, buffer->GetDesc(), range, buffer, states);
    }

    // 直接导出沿用 Builder 的版本, 内容和范围规则. 空 PassId 表示图级声明.
    template <typename Id, typename Range>
    GraphFailure RenderGraph::ExportResource(Id id, ExportUsage usage, std::optional<Range> range)
    {
        if (auto failure = CheckBuilding("Export", {}))
        {
            return failure;
        }
        try
        {
            PassBuilder builder(*this, {}, {});
            if (!builder.Export(id, usage, range))
            {
                return builder.m_Failure;
            }
            m_Exports.push_back(builder.m_Exports.front());
            return std::nullopt;
        }
        catch (const std::bad_alloc &)
        {
            return MakeError(GraphErrorCode::OutOfStorage, "Export", {}, {}, "Could not store output", id.version);
        }
        catch (const std::length_error &)
        {
            return MakeError(GraphErrorCode::OutOfStorage, "Export", {}, {}, "Output table is full", id.version);
        }
    }

    GraphFailure RenderGraph::Export(TextureId id, ExportUsage usage, std::optional<TextureRange> range)
    {
        return ExportResource(id, usage, range);
    }

    GraphFailure RenderGraph::Export(BufferId id, ExportUsage usage, std::optional<RHI::BufferRange> range)
    {
        return ExportResource(id, usage, range);
    }

    // 在一处提交生产者, 导出与回调. 表项扩容失败时撤回本次已经写入的尾部, 其余 Pass 保持原样.
    void RenderGraph::PublishPass(PassBuilder &builder, PassCallback callback)
    {
        const auto writesFor = [this](const PassUse &use) -> std::vector<VersionProducer> *
        {
            if (const auto *texture = std::get_if<TextureUse>(&use))
            {
                return texture->write ? &m_Textures[texture->access.resource.identity.index].writes : nullptr;
            }
            const auto &buffer = std::get<BufferUse>(use);
            return buffer.write ? &m_Buffers[buffer.access.resource.identity.index].writes : nullptr;
        };

        // 先登记可从尾部撤回的生产者索引, 导出和回调, 最后原位构造 Pass 声明.
        // deque 避免搬移表项中的 string/vector, 也避免搬移时分配 MSVC Debug 迭代器代理.
        const auto exportsBefore = m_Exports.size();
        const auto callbacksBefore = m_Callbacks.size();
        std::size_t indexedUses = 0;
        try
        {
            for (; indexedUses < builder.m_Uses.size(); ++indexedUses)
            {
                if (auto *writes = writesFor(builder.m_Uses[indexedUses]))
                {
                    writes->push_back({builder.m_Id, indexedUses});
                }
            }
            m_Exports.insert(m_Exports.end(), builder.m_Exports.begin(), builder.m_Exports.end());
            m_Callbacks.push_back(callback);
            m_Passes.emplace_back(builder.m_Id, builder.m_Name);
        }
        catch (...)
        {
            // 只撤回成功附加的表项. 此时候选数据仍由 AddPass 管理.
            m_Callbacks.resize(callbacksBefore);
            m_Exports.resize(exportsBefore);
            while (indexedUses != 0)
            {
                if (auto *writes = writesFor(builder.m_Uses[--indexedUses]))
                {
                    writes->pop_back();
                }
            }
            throw;
        }
        // Pass 声明就位后交换用途表, 保持生产者索引与 setup 的用途顺序一致.
        m_Passes.back().uses.swap(builder.m_Uses);
    }

    GraphFailure RenderGraph::Seal() noexcept
    {
        if (auto failure = CheckBuilding("Seal", {}))
        {
            return failure;
        }
        m_Sealed = true;
        return std::nullopt;
    }

    GraphFailure RenderGraph::InvokePass(PassId id, PassResourceResolver &resolver) const noexcept
    {
        if (!m_Sealed || m_Active)
        {
            return MakeError(GraphErrorCode::InvalidPhase, "InvokePass", {}, {}, "A sealed idle graph is required");
        }
        if (id.generation != m_Generation || id.index >= m_Passes.size())
        {
            return MakeError(GraphErrorCode::InvalidPass, "InvokePass", {}, {}, "Pass identity is not part of this graph");
        }
        // 每次调用新建一份只关联该 Pass 的查询上下文, 查询错误优先报告.
        Activity activity(m_Active);
        PassResources resources(*this, m_Passes[id.index], resolver);
        const auto &callback = m_Callbacks[id.index];
        auto failure = callback.invoke(callback.data, resources);
        return resources.m_Failure ? resources.m_Failure : failure;
    }
}
