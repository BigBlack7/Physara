#include "RenderGraph.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <vector>

#include "GraphContent.hpp"
#include "GraphRanges.hpp"

/// @file
/// @brief 实现 setup 中暂存用途, 写版本, 读回根和导出的入口, 并限制 execute 可查询的资源.
/// 声明阶段用逻辑范围和版本链判断内容. 消费阶段凭当前 Pass 的精确权限访问物理解析器.

namespace Physara::Renderer
{
    using Detail::Contains;

    // 过滤不可能的用途组合. 格式与原生视图的支持仍由编译和 RHI 判定.
    static bool AllowsRead(const RHI::TextureDesc &desc, TextureAspect aspect, GraphUsage usage) noexcept
    {
        switch (usage)
        {
        case GraphUsage::Sampled: return desc.isShaderResource;
        case GraphUsage::CopySource: return true;
        case GraphUsage::DepthStencilRead: return desc.isRenderTarget && aspect != TextureAspect::Color;
        default: return false;
        }
    }

    // 缓冲的读取按绑定角色核对描述. CopySource 无须额外的绑定标记.
    static bool AllowsRead(const RHI::BufferDesc &desc, GraphUsage usage) noexcept
    {
        switch (usage)
        {
        case GraphUsage::ShaderRead:
            return desc.structStride != 0 || desc.canHaveRawViews || desc.canHaveTypedViews;
        case GraphUsage::VertexBuffer: return desc.isVertexBuffer;
        case GraphUsage::IndexBuffer: return desc.isIndexBuffer;
        case GraphUsage::ConstantBuffer: return desc.isConstantBuffer;
        case GraphUsage::CopySource: return true;
        default: return false;
        }
    }

    // 核对附件角色和清除值类型. CopyDestination 的 Load 表达保留式区域更新.
    static bool AllowsWrite(const RHI::TextureDesc &desc, TextureAspect aspect, GraphUsage usage,
                            const TextureWriteOps &ops) noexcept
    {
        const bool color = aspect == TextureAspect::Color;
        switch (usage)
        {
        case GraphUsage::ColorAttachment:
            if (!desc.isRenderTarget || !color)
            {
                return false;
            }
            break;
        case GraphUsage::DepthStencilAttachment:
            if (!desc.isRenderTarget || color)
            {
                return false;
            }
            break;
        case GraphUsage::CopyDestination:
            // 传输 clear 在首发配置中仍需要可清除的 RenderTarget. 普通上传只写数据.
            if (ops.load == LoadOp::Clear && !desc.isRenderTarget)
            {
                return false;
            }
            break;
        default: return false;
        }

        if (ops.store > StoreOp::DontCare || ops.coverage > WriteCoverage::Full || ops.load > LoadOp::Discard)
        {
            return false;
        }
        if (ops.load != LoadOp::Clear)
        {
            return std::holds_alternative<std::monostate>(ops.clearValue);
        }
        if (!color)
        {
            const auto *clear = std::get_if<DepthStencilClear>(&ops.clearValue);
            return clear && (aspect == TextureAspect::Stencil ||
                             (clear->depth >= 0.f && clear->depth <= 1.f));
        }
        if (RHI::GetFormatInfo(desc.format).kind == RHI::FormatKind::Integer)
        {
            return std::holds_alternative<std::uint32_t>(ops.clearValue);
        }
        const auto *clear = std::get_if<RHI::Color>(&ops.clearValue);
        return clear && std::isfinite(clear->r) && std::isfinite(clear->g) &&
               std::isfinite(clear->b) && std::isfinite(clear->a);
    }

    PassBuilder::PassBuilder(const RenderGraph &graph, PassId id, std::string_view name)
        : m_Graph(graph), m_Id(id), m_Name(name)
    {
    }

    // 即使 setup 忽略某次返回值, 首个错误也会阻止整个 Pass 发布.
    bool PassBuilder::Fail(GraphErrorCode code, std::string_view name, std::uint32_t version,
                           std::string_view reason) noexcept
    {
        if (!m_Failure)
        {
            m_Failure = Detail::MakeGraphError(code, m_Id.IsValid() ? "AddPass" : "Export",
                                              m_Name, name, reason, version);
        }
        return false;
    }

    // 写版本只来自已发布链和本次 setup. 从尾部找本资源最近一次候选写入.
    template <typename Id, typename Declaration, typename Use>
    std::size_t PassBuilder::LatestVersion(Id id, const Declaration &resource) const noexcept
    {
        for (auto it = m_Uses.rbegin(); it != m_Uses.rend(); ++it)
        {
            const auto *use = std::get_if<Use>(&*it);
            if (use && use->write && use->access.resource.identity == id.identity)
            {
                return use->access.resource.version;
            }
        }
        return resource.writes.size();
    }

    // 校验身份后才能读取表项. 省略范围使用导入时确定的允许范围.
    template <typename Id, typename Range, typename Declaration, typename Use>
    std::optional<Range> PassBuilder::Validate(Id id, const std::optional<Range> &requested,
                                               const std::deque<Declaration> &resources)
    {
        if (m_Failure)
        {
            return std::nullopt;
        }
        if (!id.IsValid() || id.identity.generation != m_Graph.m_Generation || id.identity.index >= resources.size())
        {
            Fail(GraphErrorCode::InvalidResource, {}, id.version, "Resource identity is not part of this graph");
            return std::nullopt;
        }
        const auto &resource = resources[id.identity.index];
        if (id.version > resource.writes.size() && id.version > LatestVersion<Id, Declaration, Use>(id, resource))
        {
            Fail(GraphErrorCode::InvalidResource, resource.desc.debugName, id.version, "Resource version is unpublished");
            return std::nullopt;
        }
        if (requested && !Contains(resource.range, *requested))
        {
            Fail(GraphErrorCode::InvalidRange, resource.desc.debugName, id.version, "Range exceeds the resource declaration");
            return std::nullopt;
        }
        return requested.value_or(resource.range);
    }

    // Read, Load 和 Export 共用版本回溯. 本次 setup 可读到先前候选写, 再沿已发布生产者追溯.
    // 两段都使用相同的扣除规则, 每次写只解释仍未确定来源的部分. 编译阶段据此另外建立 Pass 依赖.
    template <typename Use, typename Id, typename Range, typename Declaration>
    bool PassBuilder::RequireContent(Id id, const Range &range, const Declaration &resource)
    {
        const auto readVersion = id.version;
        std::size_t pending = m_Uses.size();
        std::optional<std::vector<Range>> remaining;
        while (id.version != 0)
        {
            const Use *use = nullptr;
            if (id.version > resource.writes.size())
            {
                while (pending != 0)
                {
                    const auto *candidate = std::get_if<Use>(&m_Uses[--pending]);
                    if (candidate && candidate->write && candidate->access.resource == id)
                    {
                        use = candidate;
                        break;
                    }
                }
                assert(use != nullptr);
            }
            else
            {
                const auto &producer = resource.writes[id.version - 1];
                use = &std::get<Use>(m_Graph.m_Passes[producer.pass.index].uses[producer.useIndex]);
            }

            bool contributed = false;
            switch (Detail::ApplyWriteToQuery(range, use->access.range, Detail::DefinesContent(*use->write),
                                              remaining, contributed))
            {
            case Detail::ContentStatus::Pending: break;
            case Detail::ContentStatus::Defined: return true;
            case Detail::ContentStatus::Undefined:
                return Fail(GraphErrorCode::UndefinedContent, resource.desc.debugName, readVersion,
                            "Requested content intersects a discarded or incompletely written range");
            }
            id = use->write->previous;
        }

        // 回到版本 0 时, 只有明确有效的导入内容能填补剩余范围.
        if (resource.importState && resource.importState->contentDefined)
        {
            return true;
        }
        return Fail(GraphErrorCode::UndefinedContent, resource.desc.debugName, readVersion,
                    remaining ? "Writes did not define the entire requested range" :
                                "Requested range has no defined input content");
    }

    // 纹理和缓冲读取均先校验身份与范围, 再验证读取用途和指定版本的内容.
    bool PassBuilder::Read(TextureId id, GraphUsage usage, std::optional<TextureRange> requested)
    {
        const auto range = Validate<TextureId, TextureRange, TextureDeclaration, TextureUse>(
            id, requested, m_Graph.m_Textures);
        if (!range)
        {
            return false;
        }
        const auto &resource = m_Graph.m_Textures[id.identity.index];
        if (!AllowsRead(resource.desc, range->aspect, usage))
        {
            return Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, "Invalid texture read usage");
        }
        if (!RequireContent<TextureUse>(id, *range, resource))
        {
            return false;
        }
        m_Uses.emplace_back(TextureUse{{id, *range, usage}, std::nullopt});
        return true;
    }

    bool PassBuilder::Read(BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> requested)
    {
        const auto range = Validate<BufferId, RHI::BufferRange, BufferDeclaration, BufferUse>(
            id, requested, m_Graph.m_Buffers);
        if (!range)
        {
            return false;
        }
        const auto &resource = m_Graph.m_Buffers[id.identity.index];
        if (!AllowsRead(resource.desc, usage))
        {
            return Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, "Invalid buffer read usage");
        }
        if (!RequireContent<BufferUse>(id, *range, resource))
        {
            return false;
        }
        m_Uses.emplace_back(BufferUse{{id, *range, usage}, std::nullopt});
        return true;
    }

    // 读回标记复用本 Pass 的精确 CopySource 声明. 重复标记复用同一索引.
    // 没有匹配用途时保存候选失败, 由 AddPass 一并撤回候选内容.
    template <typename Use, typename Id, typename Range, typename Declaration>
    bool PassBuilder::MarkReadbackUse(Id id, const std::optional<Range> &requested,
                                      const std::deque<Declaration> &resources)
    {
        const auto range = Validate<Id, Range, Declaration, Use>(id, requested, resources);
        if (!range)
        {
            return false;
        }
        for (std::size_t i = 0; i < m_Uses.size(); ++i)
        {
            const auto *use = std::get_if<Use>(&m_Uses[i]);
            if (use && !use->write && use->access.usage == GraphUsage::CopySource &&
                use->access.resource == id && use->access.range == *range)
            {
                if (std::ranges::find(m_ReadbackUses, i) == m_ReadbackUses.end())
                {
                    m_ReadbackUses.push_back(i);
                }
                return true;
            }
        }
        return Fail(GraphErrorCode::InvalidUsage, resources[id.identity.index].desc.debugName, id.version,
                    "Readback requires a matching CopySource read in this Pass");
    }

    bool PassBuilder::MarkReadback(TextureId id, std::optional<TextureRange> range)
    {
        return MarkReadbackUse<TextureUse>(id, range, m_Graph.m_Textures);
    }

    bool PassBuilder::MarkReadback(BufferId id, std::optional<RHI::BufferRange> range)
    {
        return MarkReadbackUse<BufferUse>(id, range, m_Graph.m_Buffers);
    }

    // 纹理写先校验前一版本和操作合同, Load 通过后才生成候选新版本.
    TextureId PassBuilder::Write(TextureId id, GraphUsage usage, const TextureWriteOps &ops,
                                 std::optional<TextureRange> requested)
    {
        const auto range = Validate<TextureId, TextureRange, TextureDeclaration, TextureUse>(
            id, requested, m_Graph.m_Textures);
        if (!range)
        {
            return {};
        }
        const auto &resource = m_Graph.m_Textures[id.identity.index];
        if (!AllowsWrite(resource.desc, range->aspect, usage, ops))
        {
            const auto reason = ops.load == LoadOp::Clear ? "Invalid texture clear target, usage or value" :
                                                            "Invalid texture write usage or ops";
            Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, reason);
            return {};
        }
        if (id.version != LatestVersion<TextureId, TextureDeclaration, TextureUse>(id, resource))
        {
            Fail(GraphErrorCode::StaleWrite, resource.desc.debugName, id.version, "Write requires the latest version");
            return {};
        }
        if (id.version == RenderGraph::MaxIndex)
        {
            Fail(GraphErrorCode::OutOfStorage, resource.desc.debugName, id.version, "Texture version space exhausted");
            return {};
        }
        if (ops.load == LoadOp::Load && !RequireContent<TextureUse>(id, *range, resource))
        {
            return {};
        }
        const TextureId next{id.identity, id.version + 1};
        m_Uses.emplace_back(TextureUse{{next, *range, usage}, TextureWrite{id, ops}});
        return next;
    }

    // Buffer 的 CopyDestination 约定写满请求区间, 因此不要求该区间的旧内容有效.
    BufferId PassBuilder::Write(BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> requested)
    {
        const auto range = Validate<BufferId, RHI::BufferRange, BufferDeclaration, BufferUse>(
            id, requested, m_Graph.m_Buffers);
        if (!range)
        {
            return {};
        }
        const auto &resource = m_Graph.m_Buffers[id.identity.index];
        if (usage != GraphUsage::CopyDestination)
        {
            Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, "Invalid buffer write usage");
            return {};
        }
        if (id.version != LatestVersion<BufferId, BufferDeclaration, BufferUse>(id, resource))
        {
            Fail(GraphErrorCode::StaleWrite, resource.desc.debugName, id.version, "Write requires the latest version");
            return {};
        }
        if (id.version == RenderGraph::MaxIndex)
        {
            Fail(GraphErrorCode::OutOfStorage, resource.desc.debugName, id.version, "Buffer version space exhausted");
            return {};
        }
        const BufferId next{id.identity, id.version + 1};
        m_Uses.emplace_back(BufferUse{{next, *range, usage}, BufferWrite{id}});
        return next;
    }

    // 纹理和缓冲导出均核对外部用途及指定版本的内容, 成功后暂存精确范围.
    bool PassBuilder::Export(TextureId id, ExportUsage usage, std::optional<TextureRange> requested)
    {
        const auto range = Validate<TextureId, TextureRange, TextureDeclaration, TextureUse>(
            id, requested, m_Graph.m_Textures);
        if (!range)
        {
            return false;
        }
        const auto &resource = m_Graph.m_Textures[id.identity.index];
        if (usage > ExportUsage::Presentation ||
            (usage == ExportUsage::ShaderRead && !resource.desc.isShaderResource) ||
            (usage == ExportUsage::Presentation && (!resource.importState || range->aspect != TextureAspect::Color)))
        {
            return Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, "Invalid texture export usage");
        }
        if (!RequireContent<TextureUse>(id, *range, resource))
        {
            return false;
        }
        m_Exports.emplace_back(TextureExport{id, *range, usage});
        return true;
    }

    bool PassBuilder::Export(BufferId id, ExportUsage usage, std::optional<RHI::BufferRange> requested)
    {
        const auto range = Validate<BufferId, RHI::BufferRange, BufferDeclaration, BufferUse>(
            id, requested, m_Graph.m_Buffers);
        if (!range)
        {
            return false;
        }
        const auto &resource = m_Graph.m_Buffers[id.identity.index];
        if (usage >= ExportUsage::Presentation ||
            (usage == ExportUsage::ShaderRead && !AllowsRead(resource.desc, GraphUsage::ShaderRead)))
        {
            return Fail(GraphErrorCode::InvalidUsage, resource.desc.debugName, id.version, "Invalid buffer export usage");
        }
        if (!RequireContent<BufferUse>(id, *range, resource))
        {
            return false;
        }
        m_Exports.emplace_back(BufferExport{id, *range, usage});
        return true;
    }

    // 已封存的 Pass 只用自己的用途表授权. 资源范围合法后, 必须命中一条覆盖整个请求的同版本同用途声明.
    // 解析只在授权后发生. 首个失败留在当前查询上下文, 由 InvokePass 返回给执行方.
    template <typename Id, typename Range, typename Declaration, typename Use, typename ResolveFn>
    auto PassResources::ResolveDeclared(Id id, GraphUsage usage, const std::optional<Range> &requested,
                                        const std::deque<Declaration> &resources, ResolveFn resolve) noexcept
    {
        using Pointer = decltype(resolve(typename Use::AccessType{}));
        if (m_Failure)
        {
            return Pointer{};
        }
        const auto fail = [&](GraphErrorCode code, std::string_view name, std::string_view reason) -> Pointer
        {
            m_Failure = Detail::MakeGraphError(code, "PassResources", m_Pass.name, name, reason, id.version);
            return nullptr;
        };
        if (!id.IsValid() || id.identity.generation != m_Graph.m_Generation || id.identity.index >= resources.size())
        {
            return fail(GraphErrorCode::InvalidResource, {}, "Resource identity is not part of this graph");
        }
        const auto &resource = resources[id.identity.index];
        if (id.version > resource.writes.size())
        {
            return fail(GraphErrorCode::InvalidResource, resource.desc.debugName, "Resource version is unpublished");
        }

        const auto range = requested.value_or(resource.range);
        if (requested && !Contains(resource.range, range))
        {
            return fail(GraphErrorCode::InvalidRange, resource.desc.debugName, "Query range exceeds the resource");
        }
        const bool permitted = std::ranges::any_of(m_Pass.uses, [&](const PassUse &use)
        {
            const auto *declared = std::get_if<Use>(&use);
            return declared && declared->access.resource == id && declared->access.usage == usage &&
                   Contains(declared->access.range, range);
        });
        if (!permitted)
        {
            return fail(GraphErrorCode::AccessDenied, resource.desc.debugName, "Query is not declared by this Pass");
        }

        if (auto *resolved = resolve({id, range, usage}))
        {
            return resolved;
        }
        return fail(GraphErrorCode::UnresolvedResource, resource.desc.debugName, "Declared resource could not be resolved");
    }

    RHI::Texture *PassResources::Resolve(TextureId id, GraphUsage usage, std::optional<TextureRange> range) noexcept
    {
        return ResolveDeclared<TextureId, TextureRange, TextureDeclaration, TextureUse>(
            id, usage, range, m_Graph.m_Textures,
            [this](const TextureAccess &access) noexcept { return m_Resolver.Resolve(access); });
    }

    RHI::Buffer *PassResources::Resolve(BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range) noexcept
    {
        return ResolveDeclared<BufferId, RHI::BufferRange, BufferDeclaration, BufferUse>(
            id, usage, range, m_Graph.m_Buffers,
            [this](const BufferAccess &access) noexcept { return m_Resolver.Resolve(access); });
    }
}
