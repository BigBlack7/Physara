#include "GraphCompilerInternal.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "GraphContent.hpp"
#include "RenderGraph.hpp"

/// @file
/// @brief 从封存用途建立依赖, 检查全图的用途, 反馈和环, 再按外部消费确定存活顺序.
/// 图尾内容仍可交接时, 用最终顺序计算存储并发布计划. 中途诊断或分配失败均丢弃编译局部状态.

namespace Physara::Renderer::Detail
{
    // 将索引和状态值直接写入有界正文, 冷路径也无需分配字符串.
    void AppendDiagnosticNumber(Core::BoundedText &text, std::size_t value, int base) noexcept
    {
        char digits[std::numeric_limits<std::size_t>::digits10 + 1];
        const auto result = std::to_chars(digits, digits + sizeof(digits), value, base);
        text.Append({digits, static_cast<std::size_t>(result.ptr - digits)});
    }

    // 先写原因和图内索引, 再附加名称. 长名称被截断后仍可定位对应的图记录.
    GraphError CompileError(GraphErrorCode code, std::string_view reason, CompileResourceLocation resource,
                            const PassDeclaration *pass, const PassDeclaration *predecessor) noexcept
    {
        Core::BoundedText context(reason);
        context.Append(resource.kind == CompileResourceLocation::Kind::Texture ? ", textureIndex=" : ", bufferIndex=");
        AppendDiagnosticNumber(context, resource.index);
        if (pass)
        {
            context.Append(", passIndex=");
            AppendDiagnosticNumber(context, pass->id.index);
        }
        if (predecessor)
        {
            context.Append(", predecessorIndex=");
            AppendDiagnosticNumber(context, predecessor->id.index);
        }
        auto error = MakeGraphError(code, "Compile", pass ? std::string_view(pass->name) : std::string_view{},
                                    resource.name, context.View(), resource.version);
        if (predecessor && !predecessor->name.empty())
        {
            error.description.Append(", predecessor=");
            error.description.Append(predecessor->name);
        }
        return error;
    }

    /// @brief 根据已校验的纹理或缓冲访问, 取同一资源表中的记录和相应 Use 类型.
    template <typename Access, typename Inspect>
    static decltype(auto) WithResource(const RenderGraph &graph, const Access &access, Inspect &&inspect)
    {
        const auto index = access.resource.identity.index;
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(access.resource)>, TextureId>)
        {
            return inspect.template operator()<TextureUse>(graph.Textures()[index]);
        }
        else
        {
            return inspect.template operator()<BufferUse>(graph.Buffers()[index]);
        }
    }

    /// @brief Pass 间的依赖种类. 只有 RAW 是活性传播所需的内容边.
    enum class DependencyKind
    {
        Raw,
        War,
        Waw
    };

    /// @brief 用声明索引表示有向边, 资源身份及版本用于失败定位.
    /// @note 资源名借用到本次编译返回. 边经过去重后仍保留可用于诊断的一项来源.
    struct PassDependency
    {
        std::size_t from;
        std::size_t to;
        DependencyKind kind;
        CompileResourceLocation resource;
    };

    /// @brief 一次编译的依赖图及 Pass 活性, 在封存图的声明索引上工作.
    /// @invariant 边索引建成后, m_FirstOutgoing 为每个 Pass 的出边起点, 末项为总边数.
    class GraphCompiler final
    {
    public:
        static GraphResult<std::unique_ptr<const CompiledGraphPlan>> Compile(const RenderGraph &graph,
                                                                              const GraphCompileOptions &options);

    private:
        explicit GraphCompiler(const RenderGraph &graph)
            : m_Graph(graph), m_FirstOutgoing(graph.Passes().size() + 1), m_Live(graph.Passes().size())
        {
        }

        template <typename Use, typename Id, typename Range, typename Declaration>
        GraphFailure TraceContent(Id id, const Range &range, const Declaration &resource,
                                  std::optional<std::size_t> consumer);
        template <typename Use, typename Declaration>
        GraphFailure BuildUseEdges(std::size_t pass, const Use &use, const Declaration &resource);
        template <typename Use, typename Declaration>
        const Use &WriteAt(const Declaration &resource, std::size_t writeIndex) const noexcept;
        template <typename Use, typename Access, typename Declaration>
        GraphFailure CheckExport(const Access &output, const Declaration &resource) const;

        GraphFailure BuildDependencies();
        void IndexDependencies();
        [[nodiscard]] std::span<const PassDependency> Outgoing(std::size_t pass) const noexcept;
        void SortPasses(bool liveOnly, std::vector<std::size_t> &order,
                        std::vector<std::size_t> &incoming) const;
        [[nodiscard]] GraphError DescribeCycle(const std::vector<std::size_t> &incoming) const;
        void MarkLive(std::span<const std::size_t> allOrder);
        GraphFailure CheckExports() const;

        const RenderGraph &m_Graph;
        std::vector<PassDependency> m_Edges = std::vector<PassDependency>(0);
        std::vector<std::size_t> m_FirstOutgoing;
        std::vector<std::uint8_t> m_Live;
    };

    // 写索引由声明原子发布, writeIndex 对应逻辑版本 writeIndex + 1.
    template <typename Use, typename Declaration>
    const Use &GraphCompiler::WriteAt(const Declaration &resource, std::size_t writeIndex) const noexcept
    {
        const auto &producer = resource.writes[writeIndex];
        return std::get<Use>(m_Graph.Passes()[producer.pass.index].uses[producer.useIndex]);
    }

    // 从请求版本向前解释精确范围, 同一请求可能由多次写入共同定义.
    // 有 Pass 消费者时登记内容边, 图尾读取则直接标记生产者. 未决分片只能由有效导入补齐.
    template <typename Use, typename Id, typename Range, typename Declaration>
    GraphFailure GraphCompiler::TraceContent(Id id, const Range &range, const Declaration &resource,
                                              std::optional<std::size_t> consumer)
    {
        const auto location = LocateResource(id, resource.desc.debugName);
        const auto *pass = consumer ? &m_Graph.Passes()[*consumer] : nullptr;
        std::optional<std::vector<Range>> remaining;
        while (id.version != 0)
        {
            const auto writeIndex = id.version - 1;
            const auto &use = WriteAt<Use>(resource, writeIndex);
            bool contributed = false;
            const auto status = ApplyWriteToQuery(range, use.access.range, DefinesContent(*use.write),
                                                  remaining, contributed);
            if (status == ContentStatus::Undefined)
            {
                return CompileError(GraphErrorCode::UndefinedContent, "Content was discarded or incompletely written",
                                    location, pass);
            }
            if (contributed)
            {
                const auto producer = resource.writes[writeIndex].pass.index;
                if (!consumer)
                {
                    m_Live[producer] = 1;
                }
                else if (producer == *consumer)
                {
                    return CompileError(GraphErrorCode::InvalidFeedback, "Read depends on a write in the same Pass",
                                        location, pass);
                }
                else
                {
                    m_Edges.push_back({producer, *consumer, DependencyKind::Raw,
                                       LocateResource(id, resource.desc.debugName)});
                }
            }
            if (status == ContentStatus::Defined)
            {
                return std::nullopt;
            }
            id = use.write->previous;
        }
        if (resource.importState && resource.importState->contentDefined)
        {
            return std::nullopt;
        }
        return CompileError(GraphErrorCode::UndefinedContent, "Requested range has no defined input content",
                            location, pass);
    }

    // WAW 直接关联所有重叠先前写入, 避免活性裁剪中间写入后丢失首尾约束.
    // Load 从 previous 求内容边, 其 WAR 从本次写版本之后开始, 本次写由同一用途表达.
    template <typename Use, typename Declaration>
    GraphFailure GraphCompiler::BuildUseEdges(std::size_t pass, const Use &use, const Declaration &resource)
    {
        const auto &access = use.access;
        const auto location = LocateResource(access.resource, resource.desc.debugName);
        if (use.write)
        {
            for (std::size_t i = 0; i < access.resource.version - 1; ++i)
            {
                if (!Intersect(access.range, WriteAt<Use>(resource, i).access.range))
                {
                    continue;
                }
                const auto earlier = resource.writes[i].pass.index;
                if (earlier == pass)
                {
                    return CompileError(GraphErrorCode::InvalidFeedback, "Overlapping writes in the same Pass",
                                        location, &m_Graph.Passes()[pass]);
                }
                m_Edges.push_back({earlier, pass, DependencyKind::Waw, location});
            }
        }
        auto read = access.resource;
        if (use.write)
        {
            if constexpr (std::is_same_v<Use, TextureUse>)
            {
                if (use.write->ops.load != LoadOp::Load)
                {
                    return std::nullopt;
                }
                read = use.write->previous;
            }
            else
            {
                return std::nullopt;
            }
        }
        if (auto error = TraceContent<Use>(read, access.range, resource, pass))
        {
            return error;
        }
        const auto readLocation = LocateResource(read, resource.desc.debugName);
        for (std::size_t i = access.resource.version; i < resource.writes.size(); ++i)
        {
            if (!Intersect(access.range, WriteAt<Use>(resource, i).access.range))
            {
                continue;
            }
            const auto overwrite = resource.writes[i].pass.index;
            if (overwrite == pass)
            {
                return CompileError(GraphErrorCode::InvalidFeedback, "Read and overwrite overlap in the same Pass",
                                    readLocation, &m_Graph.Passes()[pass]);
            }
            m_Edges.push_back({pass, overwrite, DependencyKind::War, readLocation});
        }
        return std::nullopt;
    }

    // 先收集所有 Pass 的依赖与读回根, 再标记图尾导出所需的生产者.
    // 图尾覆盖检查等到活性确定, 此时才能知道哪些写入实际会执行.
    GraphFailure GraphCompiler::BuildDependencies()
    {
        for (const auto &pass : m_Graph.Passes())
        {
            const auto index = pass.id.index;
            m_Live[index] = !pass.readbackUses.empty();
            for (const auto &use : pass.uses)
            {
                const auto error = std::visit([&]<typename Use>(const Use &typed)
                {
                    const auto resource = typed.access.resource.identity.index;
                    if constexpr (std::is_same_v<Use, TextureUse>)
                    {
                        return BuildUseEdges(index, typed, m_Graph.Textures()[resource]);
                    }
                    else
                    {
                        return BuildUseEdges(index, typed, m_Graph.Buffers()[resource]);
                    }
                }, use);
                if (error)
                {
                    return error;
                }
            }
        }
        for (const auto &output : m_Graph.Exports())
        {
            const auto error = std::visit([&](const auto &entry)
            {
                return WithResource(m_Graph, entry, [&]<typename Use>(const auto &resource)
                {
                    return TraceContent<Use>(entry.resource, entry.range, resource, std::nullopt);
                });
            }, output);
            if (error)
            {
                return error;
            }
        }
        return std::nullopt;
    }

    // 每对 Pass 只计一次入度. 排序使 RAW 先于同一对端点的存储边, 供后续沿内容传播活性.
    // 去重后的边按起点连续存放, 用前缀位置取某个 Pass 的出边.
    void GraphCompiler::IndexDependencies()
    {
        std::ranges::sort(m_Edges, {}, [](const PassDependency &edge)
        {
            return std::tie(edge.from, edge.to, edge.kind, edge.resource.kind, edge.resource.index,
                            edge.resource.version);
        });
        const auto duplicates = std::ranges::unique(m_Edges, {}, [](const PassDependency &edge)
        {
            return std::pair{edge.from, edge.to};
        });
        m_Edges.erase(duplicates.begin(), duplicates.end());

        std::size_t cursor = 0;
        for (std::size_t pass = 0; pass < m_Live.size(); ++pass)
        {
            m_FirstOutgoing[pass] = cursor;
            while (cursor < m_Edges.size() && m_Edges[cursor].from == pass)
            {
                ++cursor;
            }
        }
        m_FirstOutgoing.back() = cursor;
    }

    // 仅在边索引建成后借用此 Pass 的连续出边.
    std::span<const PassDependency> GraphCompiler::Outgoing(std::size_t pass) const noexcept
    {
        const auto begin = m_FirstOutgoing[pass];
        return std::span(m_Edges).subspan(begin, m_FirstOutgoing[pass + 1] - begin);
    }

    // 两次排序共用最小索引堆, 每次从当前可执行集合中选择声明最早的 Pass.
    // 第二次重新计算存活端点的入度, 因为裁剪可能提前解除存储约束.
    void GraphCompiler::SortPasses(bool liveOnly, std::vector<std::size_t> &order,
                                    std::vector<std::size_t> &incoming) const
    {
        const auto included = [&](std::size_t pass) { return !liveOnly || m_Live[pass] != 0; };
        std::ranges::fill(incoming, 0);
        for (const auto &edge : m_Edges)
        {
            if (included(edge.from) && included(edge.to))
            {
                ++incoming[edge.to];
            }
        }
        order.clear();
        std::vector<std::size_t> ready(0);
        for (std::size_t pass = 0; pass < incoming.size(); ++pass)
        {
            if (included(pass) && incoming[pass] == 0)
            {
                ready.push_back(pass);
            }
        }
        const auto compare = std::greater<std::size_t>{};
        std::ranges::make_heap(ready, compare);
        while (!ready.empty())
        {
            std::ranges::pop_heap(ready, compare);
            const auto pass = ready.back();
            ready.pop_back();
            order.push_back(pass);
            for (const auto &edge : Outgoing(pass))
            {
                if (included(edge.to) && --incoming[edge.to] == 0)
                {
                    ready.push_back(edge.to);
                    std::ranges::push_heap(ready, compare);
                }
            }
        }
    }

    // 未排完的每个 Pass 仍有未排完的前驱. 逆向沿此前驱走到重复点, 报告环内的一条实际边.
    GraphError GraphCompiler::DescribeCycle(const std::vector<std::size_t> &incoming) const
    {
        std::vector<const PassDependency *> previous(incoming.size(), nullptr);
        for (const auto &edge : m_Edges)
        {
            if (incoming[edge.from] != 0 && incoming[edge.to] != 0)
            {
                previous[edge.to] = &edge;
            }
        }
        std::vector<std::uint8_t> seen(incoming.size());
        auto pass = static_cast<std::size_t>(std::ranges::find_if(incoming, [](auto n) { return n != 0; }) -
                                              incoming.begin());
        while (!seen[pass])
        {
            seen[pass] = 1;
            pass = previous[pass]->from;
        }
        const auto &edge = *previous[pass];
        const auto reason = edge.kind == DependencyKind::War ? "Cyclic dependency through WAR overwrite" :
                            edge.kind == DependencyKind::Waw ? "Cyclic dependency through WAW overlap" :
                                                                "Cyclic dependency through RAW read";
        return CompileError(GraphErrorCode::CyclicDependency, reason, edge.resource,
                            &m_Graph.Passes()[edge.to], &m_Graph.Passes()[edge.from]);
    }

    // 全图已无环. 逆序遍历可让每个生产者看到其内容消费者的最终活性.
    void GraphCompiler::MarkLive(std::span<const std::size_t> allOrder)
    {
        for (std::size_t i = allOrder.size(); i != 0; --i)
        {
            const auto pass = allOrder[i - 1];
            if (!m_Live[pass] && std::ranges::any_of(Outgoing(pass), [&](const PassDependency &edge)
            {
                return edge.kind == DependencyKind::Raw && m_Live[edge.to] != 0;
            }))
            {
                m_Live[pass] = 1;
            }
        }
    }

    // 图尾消费不能挪到覆写之前. 仅有存活且相交的较新写入会破坏所请求版本.
    template <typename Use, typename Access, typename Declaration>
    GraphFailure GraphCompiler::CheckExport(const Access &output, const Declaration &resource) const
    {
        for (std::size_t i = output.resource.version; i < resource.writes.size(); ++i)
        {
            const auto pass = resource.writes[i].pass.index;
            if (m_Live[pass] && Intersect(output.range, WriteAt<Use>(resource, i).access.range))
            {
                return CompileError(GraphErrorCode::OverwrittenExport, "A live newer write overwrites the export",
                                    LocateResource(output.resource, resource.desc.debugName), &m_Graph.Passes()[pass]);
            }
        }
        return std::nullopt;
    }

    // 所有导出都必须在最终存活序列之后取得声明内容, 包括直接导出的导入资源.
    GraphFailure GraphCompiler::CheckExports() const
    {
        for (const auto &output : m_Graph.Exports())
        {
            const auto error = std::visit([&](const auto &entry)
            {
                return WithResource(m_Graph, entry, [&]<typename Use>(const auto &resource)
                {
                    return CheckExport<Use>(entry, resource);
                });
            }, output);
            if (error)
            {
                return error;
            }
        }
        return std::nullopt;
    }

    // 全图校验和裁剪后排序均使用临时记录. 存储规划成功后才移交同图代只读计划.
    GraphResult<std::unique_ptr<const CompiledGraphPlan>> GraphCompiler::Compile(const RenderGraph &graph,
                                                                                 const GraphCompileOptions &options)
    {
        if (!graph.m_Sealed || graph.m_Active)
        {
            return CompileError(GraphErrorCode::InvalidPhase, "A sealed idle graph is required");
        }
        std::vector<PlannedGraphPass> passes(graph.Passes().size());
        if (auto error = BuildPassPlans(graph, options, passes))
        {
            return *error;
        }
        GraphCompiler compiler(graph);
        if (auto error = compiler.BuildDependencies())
        {
            return *error;
        }
        compiler.IndexDependencies();
        std::vector<std::size_t> order(0);
        order.reserve(passes.size());
        std::vector<std::size_t> incoming(passes.size());
        compiler.SortPasses(false, order, incoming);
        if (order.size() != passes.size())
        {
            return compiler.DescribeCycle(incoming);
        }
        compiler.MarkLive(order);
        compiler.SortPasses(true, order, incoming);
        assert(std::ranges::count(compiler.m_Live, 1) == static_cast<std::ptrdiff_t>(order.size()));
        if (auto error = compiler.CheckExports())
        {
            return *error;
        }

        auto plan = std::make_unique<CompiledGraphPlan>();
        plan->graphGeneration = graph.m_Generation;
        if (!graph.Exports().empty())
        {
            plan->exports.assign(graph.Exports().begin(), graph.Exports().end());
        }
        plan->passes.resize(order.size());
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            plan->passes[i] = std::move(passes[order[i]]);
        }
        if (auto error = BuildStoragePlan(graph, options.transientByteBudget, *plan))
        {
            return *error;
        }
        return std::unique_ptr<const CompiledGraphPlan>(std::move(plan));
    }
}

namespace Physara::Renderer
{
    // 将编译临时存储可传播的分配或容量失败转为自持诊断.
    GraphResult<std::unique_ptr<const CompiledGraphPlan>> CompileRenderGraph(const RenderGraph &graph,
                                                                             const GraphCompileOptions &options) noexcept
    {
        try
        {
            return Detail::GraphCompiler::Compile(graph, options);
        }
        catch (const std::bad_alloc &)
        {
            return Detail::CompileError(GraphErrorCode::OutOfStorage, "Could not allocate CPU planning storage");
        }
        catch (const std::length_error &)
        {
            return Detail::CompileError(GraphErrorCode::OutOfStorage, "CPU plan exceeds container capacity");
        }
    }
}
