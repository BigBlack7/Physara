#include "GraphExecutor.hpp"

/// @file
/// @brief 按编译顺序物化并消费存活 Pass. 资源状态交接和已发工作留在同一次即时录制.

#include <algorithm>
#include <cassert>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <ImmediateGraphics.hpp>

#include "GraphContent.hpp"
#include "GraphExecutionStates.hpp"
#include "GraphPoolInternal.hpp"
#include "GraphRanges.hpp"

namespace Physara::Renderer::Detail
{
    using RHI::ResourceStates;

    /// @brief 将封存图身份映射到导入对象或首次物化的瞬态槽, 并按计划驱动命令列表.
    /// @note 槽表持有本次执行的物理对象, 图持有导入引用. RHI 包另行保活已登记或已用的对象.
    class GraphExecutor final : private PassResourceResolver
    {
    public:
        /// @brief 校验计划与录制, 配对接管和归还, 并在恢复成功后交付结果.
        [[nodiscard]] static GraphResult<GraphExecutionOutputs> Run(
            const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
            RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics, GraphPoolReservation *reservation);

    private:
        /// @brief 接管并执行一次录制. 返回后由 Run 结清已经消费的预留.
        [[nodiscard]] static GraphResult<GraphExecutionOutputs> RunRecording(
            const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
            RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics, GraphPoolReservation *reservation);
        using Slot = std::variant<std::monostate, RHI::TextureHandle, RHI::BufferHandle>;
        static constexpr std::size_t Unused = (std::numeric_limits<std::size_t>::max)();

        GraphExecutor(const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
                      RHI::CommandList &commands, RHI::GraphRecording &recording, GraphExecutionDiagnostics diagnostics,
                      GraphPoolReservation *reservation)
            : m_Graph(graph), m_Plan(plan), m_Device(device), m_Commands(commands), m_Recording(recording),
              m_Diagnostics(diagnostics), m_Reservation(reservation),
              m_Slots(plan.transientSlots.size()), m_Textures(graph.Textures().size(), Unused),
              m_Buffers(graph.Buffers().size(), Unused), m_TextureImports(plan.textures.size(), 0),
              m_BufferImports(plan.buffers.size(), 0), m_Outputs(std::make_unique<GraphExecutionOutputList>(0))
        {
            // 提前为导出表预留元素. 可写纹理的覆盖表和只读出口的范围持有在图尾建立, 仍可能分配失败.
            m_Outputs->reserve(plan.exports.size());
            // 计划的存活表按声明索引递增但省略了裁剪资源, 只填本次会被访问的路由.
            for (std::size_t i = 0; i < plan.textures.size(); ++i)
            {
                m_Textures[plan.textures[i].resource.Identity().index] = i;
            }
            for (std::size_t i = 0; i < plan.buffers.size(); ++i)
            {
                m_Buffers[plan.buffers[i].resource.Identity().index] = i;
            }
        }

        [[nodiscard]] RHI::Texture *Resolve(const TextureAccess &access) noexcept override
        {
            return Texture(access.resource);
        }
        [[nodiscard]] RHI::Buffer *Resolve(const BufferAccess &access) noexcept override
        {
            return Buffer(access.resource);
        }
        /// @brief 通过存活资源路由借用同一逻辑资源的物理对象. 所有版本共用该路由.
        /// @pre 该资源已在当前或更早的计划位置物化.
        [[nodiscard]] RHI::Texture *Texture(TextureId id) const noexcept;
        [[nodiscard]] RHI::Buffer *Buffer(BufferId id) const noexcept;
        /// @brief 在资源的 firstUse 建立导入前态或首次创建物理槽. 同一槽的后任沿用已建立的状态.
        [[nodiscard]] GraphFailure Materialize(TextureId id, std::size_t position);
        [[nodiscard]] GraphFailure Materialize(BufferId id, std::size_t position);
        /// @brief 在首用取得或者登记本计划的实际物理对象.
        template <typename Handle, typename Create>
        [[nodiscard]] Handle Acquire(std::optional<std::size_t> slot, Create create,
                                     const RHI::GraphicsResourceUse *&previous);
        /// @brief 从本次录制及物理槽生成可供 caller 继续持有的出口.
        template <typename Export, typename Resource>
        [[nodiscard]] GraphRetainedResource RetainOutput(const Export &output, const Resource &resource);
        /// @brief 在交付出口前为可写对象建立内容账目, 使同次只读出口也能登记保护范围.
        void PrepareWritableOutputs();
        /// @brief 修改可更新导入的一次写范围, 只在其可能开始写入或结束写入时调用.
        void UpdateMutableContent(const TextureUse &use, bool defined) noexcept;
        /// @brief 在执行回调边界更新它可修改的范围. 成功时按各写的 Store 效果登记.
        void UpdateMutableContents(const PlannedGraphPass &pass, bool succeeded) noexcept;
        /// @brief 在录制接管前拒绝存活写破坏尚存只读出口.
        [[nodiscard]] static GraphFailure CheckRetainedWrites(const RenderGraph &graph,
                                                              const CompiledGraphPlan &plan) noexcept;
        /// @brief 接管前核对存活导入的设备, 池和已接受观察.
        /// @note 观察推进后, 内容有保证的纹理逐 mip/layer 复核入口. 本包的实际使用在首用时登记.
        template <typename Planned, typename Declared>
        [[nodiscard]] static bool RetainedImportsMatch(const Planned &planned, const Declared &declared,
                                                       RHI::Device &device, GraphPoolReservation *reservation) noexcept;
        /// @brief 与已跟踪状态核对 owner 声明的入口, 并为未知追踪单元登记已知前态.
        [[nodiscard]] GraphFailure TrackImport(RHI::Texture &texture, const PlannedGraphTexture &resource);
        [[nodiscard]] GraphFailure TrackImport(RHI::Buffer &buffer, const PlannedGraphBuffer &resource);
        /// @brief 将计划附件转换为 RHI Framebuffer 描述, RHI 按实际对象检查兼容性并创建句柄.
        [[nodiscard]] RHI::FramebufferHandle PrepareFramebuffer(const PlannedGraphPass &pass);
        /// @brief 为每个 typed clear 单独提交传输状态, 并以计划选定的子资源和分量清除.
        [[nodiscard]] GraphFailure ApplyClears(const PlannedGraphPass &pass);
        /// @brief 以同一阶段合并并提交该 Pass 的普通用途.
        [[nodiscard]] GraphFailure PrepareOrdinaryUses(const PlannedGraphPass &pass);
        /// @brief 首用物化和 recipe 准备成功后才处理清除, 普通用途和同步执行回调.
        [[nodiscard]] GraphFailure ExecutePass(const PlannedGraphPass &pass, std::size_t position);
        /// @brief 在同一录制中完成已使用导入和瞬态输出的边界请求, 句柄暂存至接管成功归还.
        [[nodiscard]] GraphFailure Handoff();
        /// @brief 遍历编译顺序并收口阶段失败. 录制 owner 继续持有已发工作的引用.
        [[nodiscard]] GraphFailure Record();
        /// @brief 准备已报首错优先于后续返回值或异常. 无先前诊断时返回空.
        [[nodiscard]] GraphFailure PreparationFailure(const PassResources &resources) const noexcept;
        /// @brief 复制当前阶段和 Pass 上下文, 使错误可跨本次执行存活.
        [[nodiscard]] GraphError Error(GraphErrorCode code, std::string_view stage, std::string_view name,
                                      std::string_view reason, std::optional<std::uint32_t> version = {}) const noexcept;

        const RenderGraph &m_Graph;
        const CompiledGraphPlan &m_Plan;
        RHI::Device &m_Device;
        RHI::CommandList &m_Commands;
        RHI::GraphRecording &m_Recording;
        GraphExecutionDiagnostics m_Diagnostics;
        GraphPoolReservation *m_Reservation;
        std::vector<Slot> m_Slots;
        // 以原图声明索引查询紧凑计划表. 导入登记标记按计划索引保存, 同一 Pass 可能多次使用同一资源.
        std::vector<std::size_t> m_Textures;
        std::vector<std::size_t> m_Buffers;
        std::vector<std::uint8_t> m_TextureImports;
        std::vector<std::uint8_t> m_BufferImports;
        std::unique_ptr<GraphExecutionOutputList> m_Outputs;
        std::string_view m_CurrentPass;
        std::string_view m_Stage = "GraphExec.Preflight";
    };

    GraphError GraphExecutor::Error(GraphErrorCode code, std::string_view stage, std::string_view name,
                                    std::string_view reason, std::optional<std::uint32_t> version) const noexcept
    {
        return MakeGraphError(code, stage, m_CurrentPass, name, reason, version);
    }

    RHI::Texture *GraphExecutor::Texture(TextureId id) const noexcept
    {
        const auto index = id.Identity().index;
        assert(m_Textures[index] != Unused);
        const auto slot = m_Plan.textures[m_Textures[index]].transientSlot;
        return slot ? std::get<RHI::TextureHandle>(m_Slots[*slot]).Get() : m_Graph.Textures()[index].imported.Get();
    }

    RHI::Buffer *GraphExecutor::Buffer(BufferId id) const noexcept
    {
        const auto index = id.Identity().index;
        assert(m_Buffers[index] != Unused);
        const auto slot = m_Plan.buffers[m_Buffers[index]].transientSlot;
        return slot ? std::get<RHI::BufferHandle>(m_Slots[*slot]).Get() : m_Graph.Buffers()[index].imported.Get();
    }

    GraphFailure GraphExecutor::TrackImport(RHI::Texture &texture, const PlannedGraphTexture &resource)
    {
        const auto &declared = m_Graph.Textures()[resource.resource.Identity().index];
        const auto &range = resource.range;
        const auto &name = texture.GetDesc().debugName;
        // Observe 以最近已接受的整对象状态登记本包. 下方只在图声明范围内沿预检核实的入口接入 tracker.
        if (declared.retained && !declared.retained.m_Entry->Observe(m_Commands))
        {
            return Error(GraphErrorCode::StateMismatch, m_Stage, name, "Cannot observe retained Texture in this list");
        }
        // 已知状态来自同一包或 keepInitialState. 普通导入使用一致状态, 池导入可逐单元接续.
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                const auto entry = declared.importUse ? TextureStateAt(*declared.importUse, declared.desc, layer, mip)
                    : resource.importState->entry;
                const auto prior = m_Commands.GetTextureSubresourceState(&texture, layer, mip);
                if (!m_Diagnostics.IsClear())
                {
                    return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "GetTextureSubresourceState failed");
                }
                if (prior != ResourceStates::Unknown && prior != entry)
                {
                    return Error(GraphErrorCode::StateMismatch, m_Stage, name,
                                 "Import entry disagrees with Texture state");
                }
                if (prior == ResourceStates::Unknown)
                {
                    m_Recording.BeginTracking(texture, {mip, 1, layer, 1}, entry);
                    if (!m_Diagnostics.IsClear())
                    {
                        return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "BeginTrackingTextureState failed");
                    }
                }
            }
        }
        return std::nullopt;
    }

    GraphFailure GraphExecutor::TrackImport(RHI::Buffer &buffer, const PlannedGraphBuffer &resource)
    {
        const auto &declared = m_Graph.Buffers()[resource.resource.Identity().index];
        const auto entry = resource.importState->entry;
        const auto &desc = buffer.GetDesc();
        if (declared.retained && !declared.retained.m_Entry->Observe(m_Commands))
        {
            return Error(GraphErrorCode::StateMismatch, m_Stage, desc.debugName, "Cannot observe retained Buffer");
        }
        auto prior = m_Commands.GetBufferState(&buffer);
        if (!m_Diagnostics.IsClear())
        {
            return Error(GraphErrorCode::ExecutionFailed, m_Stage, desc.debugName, "GetBufferState failed");
        }
        if (prior == ResourceStates::Unknown && desc.keepInitialState)
        {
            // Buffer 的查询只读包内账目. 尚无记录时, keepInitialState 由首次用途沿描述建立.
            prior = desc.initialState;
        }
        if (prior == ResourceStates::Unknown)
        {
            m_Recording.BeginTracking(buffer, entry);
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, desc.debugName, "BeginTrackingBufferState failed");
            }
        }
        else if (prior != entry)
        {
            return Error(GraphErrorCode::StateMismatch, m_Stage, desc.debugName, "Import entry disagrees with Buffer state");
        }
        return std::nullopt;
    }

    template <typename Handle, typename Create>
    Handle GraphExecutor::Acquire(std::optional<std::size_t> slot, Create create,
                                  const RHI::GraphicsResourceUse *&previous)
    {
        if (!m_Reservation)
        {
            return create();
        }
        auto &entry = *m_Reservation->m_Slots[*slot];
        auto &stats = m_Reservation->m_Pool.m_Statistics;
        // 先借出旧的接受状态, 再登记新包. Observe 在换包时将旧观察保留在同一池项中.
        previous = entry.AcceptedUse();
        // 已完成且无持有的槽开始新的逻辑寿命. 接续物理状态, 清除旧内容和读者登记.
        entry.mutableContent.reset();
        using Resource = std::remove_pointer_t<decltype(std::declval<Handle>().Get())>;
        Handle handle;
        if (entry.object)
        {
            handle = static_cast<Resource *>(entry.object.Get());
            ++stats.hits;
        }
        else
        {
            handle = create();
            if (!handle)
            {
                return {};
            }
            entry.object = handle.Get();
            ++stats.created;
        }
        return entry.Observe(m_Commands) ? handle : Handle{};
    }

    GraphFailure GraphExecutor::Materialize(TextureId id, std::size_t position)
    {
        const auto route = m_Textures[id.Identity().index];
        const auto &resource = m_Plan.textures[route];
        if (resource.firstUse != position)
        {
            return std::nullopt;
        }
        const auto &declared = m_Graph.Textures()[id.Identity().index];
        if (!resource.transientSlot)
        {
            if (!m_TextureImports[route])
            {
                if (auto failure = TrackImport(*declared.imported.Get(), resource))
                {
                    return failure;
                }
                m_TextureImports[route] = true;
            }
            return std::nullopt;
        }

        auto &slot = m_Slots[*resource.transientSlot];
        if (std::holds_alternative<std::monostate>(slot))
        {
            const auto &desc = std::get<RHI::TextureDesc>(m_Plan.transientSlots[*resource.transientSlot].desc);
            const RHI::GraphicsResourceUse *previous = nullptr;
            auto texture = Acquire<RHI::TextureHandle>(resource.transientSlot,
                [&] { return m_Device.CreateTexture(desc); }, previous);
            if (!texture || !m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName, "Texture acquire failed",
                             id.version);
            }
            if (previous)
            {
                // 一致末态可一次登记全纹理. 尚有差异时保留每个 mip/layer 的入口, 内容仍由新版本声明.
                const auto first = previous->states.front();
                if (std::ranges::all_of(previous->states, [first](auto state) { return state == first; }))
                {
                    m_Recording.BeginTracking(*texture.Get(), RHI::AllSubresources, first);
                }
                else
                {
                    for (std::uint32_t layer = 0; layer < desc.arraySize && m_Diagnostics.IsClear(); ++layer)
                    {
                        for (std::uint32_t mip = 0; mip < desc.mipLevels && m_Diagnostics.IsClear(); ++mip)
                        {
                            m_Recording.BeginTracking(*texture.Get(), {mip, 1, layer, 1},
                                TextureStateAt(*previous, desc, layer, mip));
                        }
                    }
                }
            }
            else if (!desc.keepInitialState)
            {
                // 为无自动初态协议的新存储登记 Common. keepInitialState 由 tracker 处理首次和后继录制.
                m_Recording.BeginTracking(*texture.Get(), RhiSubresources(resource.range), ResourceStates::Common);
            }
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName,
                             "BeginTrackingTextureState failed", id.version);
            }
            slot = std::move(texture);
        }
        return std::nullopt;
    }

    GraphFailure GraphExecutor::Materialize(BufferId id, std::size_t position)
    {
        const auto route = m_Buffers[id.Identity().index];
        const auto &resource = m_Plan.buffers[route];
        if (resource.firstUse != position)
        {
            return std::nullopt;
        }
        const auto &declared = m_Graph.Buffers()[id.Identity().index];
        if (!resource.transientSlot)
        {
            if (!m_BufferImports[route])
            {
                if (auto failure = TrackImport(*declared.imported.Get(), resource))
                {
                    return failure;
                }
                m_BufferImports[route] = true;
            }
            return std::nullopt;
        }

        auto &slot = m_Slots[*resource.transientSlot];
        if (std::holds_alternative<std::monostate>(slot))
        {
            const auto &desc = std::get<RHI::BufferDesc>(m_Plan.transientSlots[*resource.transientSlot].desc);
            const RHI::GraphicsResourceUse *previous = nullptr;
            auto buffer = Acquire<RHI::BufferHandle>(resource.transientSlot,
                [&] { return m_Device.CreateBuffer(desc); }, previous);
            if (!buffer || !m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName, "Buffer acquire failed",
                             id.version);
            }
            if (previous || !desc.keepInitialState)
            {
                const auto entry = previous ? previous->states.front() :
                    desc.initialState == ResourceStates::Unknown ? ResourceStates::Common : desc.initialState;
                m_Recording.BeginTracking(*buffer.Get(), entry);
                if (!m_Diagnostics.IsClear())
                {
                    return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName,
                                 "BeginTrackingBufferState failed", id.version);
                }
            }
            slot = std::move(buffer);
        }
        // 非空槽已由前一逻辑资源物化. 当前 Pass 的显式用途从包内真实状态接续.
        return std::nullopt;
    }

    template <typename Export, typename Resource>
    GraphRetainedResource GraphExecutor::RetainOutput(const Export &output, const Resource &resource)
    {
        GraphRetainedResource retained;
        if (m_Reservation && resource.transientSlot)
        {
            retained.m_Entry = m_Reservation->m_Slots[*resource.transientSlot];
        }
        else if constexpr (std::is_same_v<Export, TextureExport>)
        {
            retained = m_Graph.Textures()[output.resource.Identity().index].retained;
        }
        else
        {
            retained = m_Graph.Buffers()[output.resource.Identity().index].retained;
        }
        if (retained)
        {
            // 导入再导出沿用物理项, 但出口关联当前录制. 只有这次提交被接受后才能核对它的票据.
            retained.m_Produced = retained.m_Entry->use;
            retained.m_Range = output.range;
            retained.m_Usage = output.usage;
            retained.m_OwnerWrite = output.ownerWrite;
            if constexpr (std::is_same_v<Export, TextureExport>)
            {
                if (auto &content = retained.m_Entry->mutableContent; content)
                {
                    if (output.ownerWrite)
                    {
                        // 可更新的 owner 跟随当前内容. 同一批次需要固定内容的读者持有自己的范围登记.
                        retained.m_ContentPin.reset();
                        content->SetDefined(output.range, true);
                    }
                    else
                    {
                        retained.m_ContentPin = content->Pin(output.range);
                    }
                }
            }
        }
        return retained;
    }

    void GraphExecutor::PrepareWritableOutputs()
    {
        if (!m_Reservation)
        {
            return;
        }
        // 同一存储可同时导出只读和可写范围. 先建立内容表, 让排在可写出口之前的只读出口也能登记.
        for (const auto &output : m_Plan.exports)
        {
            if (const auto *texture = std::get_if<TextureExport>(&output); texture && texture->ownerWrite)
            {
                const auto id = texture->resource;
                const auto slot = m_Plan.textures[m_Textures[id.Identity().index]].transientSlot;
                if (slot)
                {
                    auto &content = m_Reservation->m_Slots[*slot]->mutableContent;
                    if (!content)
                    {
                        content.emplace(Texture(id)->GetDesc());
                    }
                }
            }
        }
    }

    void GraphExecutor::UpdateMutableContent(const TextureUse &use, bool defined) noexcept
    {
        const auto &entry = m_Graph.Textures()[use.access.resource.Identity().index].retained.m_Entry;
        if (entry && entry->mutableContent)
        {
            entry->mutableContent->SetDefined(use.access.range, defined);
        }
    }

    void GraphExecutor::UpdateMutableContents(const PlannedGraphPass &pass, bool succeeded) noexcept
    {
        for (const auto &planned : pass.uses)
        {
            const auto *use = std::get_if<TextureUse>(&planned.declaration);
            if (use && use->write)
            {
                UpdateMutableContent(*use, succeeded && DefinesContent(*use->write));
            }
        }
    }

    GraphFailure GraphExecutor::CheckRetainedWrites(const RenderGraph &graph, const CompiledGraphPlan &plan) noexcept
    {
        for (const auto &pass : plan.passes)
        {
            for (const auto &planned : pass.uses)
            {
                const auto *use = std::get_if<TextureUse>(&planned.declaration);
                if (use && use->write)
                {
                    const auto id = use->access.resource;
                    const auto &texture = graph.Textures()[id.Identity().index];
                    const auto &entry = texture.retained.m_Entry;
                    if (entry && entry->mutableContent && entry->mutableContent->HasReader(use->access.range))
                    {
                        return MakeGraphError(GraphErrorCode::AccessDenied, "GraphExec.Preflight",
                            graph.Passes()[pass.id.index].name, texture.desc.debugName,
                            "Texture write overlaps a retained read-only output", id.version);
                    }
                }
            }
        }
        return std::nullopt;
    }

    RHI::FramebufferHandle GraphExecutor::PrepareFramebuffer(const PlannedGraphPass &pass)
    {
        RHI::FramebufferDesc desc;
        const auto view = [&](const PlannedGraphAttachment &attachment) -> RHI::FramebufferAttachment
        {
            // 状态可以按同一 mip/layer 合并, 但原生 FBO 仍需按计划选择实际挂载分量.
            return {Texture(attachment.resource), RhiSubresources(attachment.range), attachment.format,
                    attachment.readOnly, attachment.range.aspect};
        };
        for (const auto &color : pass.colorAttachments)
        {
            desc.AddColorAttachment(view(color));
        }
        if (pass.depthStencilAttachment)
        {
            desc.SetDepthAttachment(view(*pass.depthStencilAttachment));
        }
        return m_Device.CreateFramebuffer(desc);
    }

    GraphFailure GraphExecutor::ApplyClears(const PlannedGraphPass &pass)
    {
        for (const auto &clear : pass.clears)
        {
            const auto &use = std::get<TextureUse>(pass.uses[clear.useIndex].declaration);
            const auto &id = use.access.resource;
            auto *texture = Texture(id);
            const auto range = RhiSubresources(use.access.range);
            const auto &name = m_Graph.Textures()[id.Identity().index].desc.debugName;
            m_Recording.RequestState(*texture, range, clear.state);
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "SetTextureState for clear failed", id.version);
            }
            const RHI::GraphTextureUse access{texture, range, use.access.range.aspect, clear.state};
            m_Recording.CommitUses(std::span(&access, 1), {});
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "CommitBarriers for clear failed", id.version);
            }
            const auto &value = use.write->ops.clearValue;
            // 状态准备成功后, 只有即将清除的范围可能改变. 尚未走到的清除仍保留入口覆盖.
            UpdateMutableContent(use, false);
            // RHI 状态范围使用 mip/layer. 清除命令还需要原写入的数值类型和 depth/stencil aspect.
            if (const auto *color = std::get_if<RHI::Color>(&value))
            {
                m_Commands.ClearTextureFloat(texture, range, *color);
            }
            else if (const auto *integer = std::get_if<std::uint32_t>(&value))
            {
                m_Commands.ClearTextureUInt(texture, range, *integer);
            }
            else
            {
                const auto &depthStencil = std::get<DepthStencilClear>(value);
                const auto aspect = use.access.range.aspect;
                m_Commands.ClearDepthStencilTexture(texture, range, aspect != TextureAspect::Stencil,
                    depthStencil.depth, aspect != TextureAspect::Depth, depthStencil.stencil);
            }
            m_Recording.RevokeUses();
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "Typed clear failed", id.version);
            }
        }
        return std::nullopt;
    }

    // 读和写都请求同一个 UAV 状态. 保留各声明的方向, 供录制在实际 Dispatch 前核对活动绑定.
    static RHI::StorageAccess StorageDirection(GraphUsage usage) noexcept
    {
        switch (usage)
        {
        case GraphUsage::StorageRead: return RHI::StorageAccess::Read;
        case GraphUsage::StorageWrite: return RHI::StorageAccess::Write;
        case GraphUsage::StorageReadWrite: return RHI::StorageAccess::ReadWrite;
        default: return RHI::StorageAccess::None;
        }
    }

    GraphFailure GraphExecutor::PrepareOrdinaryUses(const PlannedGraphPass &pass)
    {
        // 跟踪状态按物理单元合并. 授权保留每条原始用途和范围, 深度采样位也与附件读取权限分开登记.
        GraphExecutionStates uses;
        std::vector<RHI::GraphTextureUse> textures(0);
        std::vector<RHI::GraphBufferUse> buffers(0);
        for (const auto &planned : pass.uses)
        {
            if (!planned.state)
            {
                continue;
            }
            if (const auto *texture = std::get_if<TextureUse>(&planned.declaration))
            {
                const auto &access = texture->access;
                auto *object = Texture(access.resource);
                uses.Add(*object, access.range, *planned.state);
                const auto state = access.usage == GraphUsage::Sampled ? ResourceStates::ShaderResource : *planned.state;
                textures.push_back({object, RhiSubresources(access.range), access.range.aspect, state,
                                    StorageDirection(access.usage)});
            }
            else
            {
                const auto &buffer = std::get<BufferUse>(planned.declaration);
                const auto &access = buffer.access;
                auto *object = Buffer(access.resource);
                uses.Add(*object, *planned.state);
                // 行范围随本 Pass 授权. 结构化表另携带布局要求的绑定范围, 供后端核对预建集合.
                buffers.push_back({object, access.range, *planned.state, buffer.indexedBinding,
                                   StorageDirection(access.usage)});
            }
        }
        return uses.Commit(m_Recording, m_Diagnostics, m_Stage, m_CurrentPass, textures, buffers);
    }

    GraphFailure GraphExecutor::PreparationFailure(const PassResources &resources) const noexcept
    {
        if (resources.m_Failure)
        {
            return resources.m_Failure;
        }
        if (!m_Diagnostics.IsClear())
        {
            return Error(GraphErrorCode::ExecutionFailed, m_Stage, {}, "RHI reported Error/Fatal during preparation");
        }
        return std::nullopt;
    }

    GraphFailure GraphExecutor::ExecutePass(const PlannedGraphPass &pass, std::size_t position)
    {
        const auto &declaration = m_Graph.Passes()[pass.id.index];
        m_CurrentPass = declaration.name;
        m_Stage = "GraphExec.Materialize";
        for (const auto &use : pass.uses)
        {
            auto failure = std::visit([&](const auto &item)
            {
                return Materialize(item.access.resource, position);
            }, use.declaration);
            if (failure)
            {
                return failure;
            }
        }

        m_Stage = "GraphExec.Prepare";
        // 将 FBO 和集合 owner 保留到 execute 返回. 原生消费发生后, 列表负责随包保活实际依赖.
        PassResources resources(m_Graph, declaration, *this);
        resources.m_Diagnostics = m_Diagnostics;
        resources.m_RhiStage = m_Stage;
        RHI::FramebufferHandle framebuffer;
        if (!pass.colorAttachments.empty() || pass.depthStencilAttachment)
        {
            framebuffer = PrepareFramebuffer(pass);
            if (!framebuffer || !m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, {}, "CreateFramebuffer failed");
            }
            resources.m_Framebuffer = framebuffer.Get();
        }
        PassPreparation preparation(m_Device, resources);
        const auto &callback = m_Graph.m_Callbacks[pass.id.index];
        if (callback.prepare)
        {
            GraphFailure failure;
            try
            {
                failure = callback.prepare(callback.data, preparation);
            }
            catch (...)
            {
                // 查询及底层错误的诊断保存在本次准备对象中, 须在异常展开销毁它之前取出.
                if (auto first = PreparationFailure(resources))
                {
                    return first;
                }
                throw;
            }
            if (auto first = PreparationFailure(resources))
            {
                return first;
            }
            if (failure)
            {
                return failure;
            }
            resources.m_BindingSets = preparation.m_BindingSets;
        }

        m_Stage = "GraphExec.Clear";
        // recipe 全部就绪后才修改内容. 清除和普通用途各自提交, 同一附件随后可按 Draw 用途消费.
        if (auto failure = ApplyClears(pass))
        {
            return failure;
        }
        m_Stage = "GraphExec.Uses";
        if (auto failure = PrepareOrdinaryUses(pass))
        {
            return failure;
        }
        m_Stage = "GraphExec.Pass";
        resources.m_RhiStage = m_Stage;
        // 普通用途准备只建立状态和权限. 进入可能写入的回调时才撤销其目标覆盖.
        UpdateMutableContents(pass, false);
        auto failure = m_Graph.InvokePreparedPass(resources, &m_Commands);
        if (!failure)
        {
            UpdateMutableContents(pass, true);
        }
        // 下一个 Pass 从自己的准备阶段开始. 上个回调的访问范围到这里失效, 已发工作的引用仍随包保留.
        m_Recording.RevokeUses();
        return failure;
    }

    GraphFailure GraphExecutor::Handoff()
    {
        m_CurrentPass = {};
        m_Stage = "GraphExec.Handoff";
        PrepareWritableOutputs();
        GraphExecutionStates uses;
        // 只遍历计划中的导入. 图尾直接导出时在这里登记前态, 此前已参与 Pass 的对象沿原记录接续.
        for (const auto &resource : m_Plan.textures)
        {
            if (!resource.importState)
            {
                continue;
            }
            if (auto failure = Materialize(resource.resource, m_Plan.passes.size()))
            {
                return failure;
            }
            uses.Add(*Texture(resource.resource), resource.range, resource.importState->final);
        }
        for (const auto &resource : m_Plan.buffers)
        {
            if (!resource.importState)
            {
                continue;
            }
            if (auto failure = Materialize(resource.resource, m_Plan.passes.size()))
            {
                return failure;
            }
            uses.Add(*Buffer(resource.resource), resource.importState->final);
        }
        for (const auto &entry : m_Plan.exports)
        {
            if (const auto *texture = std::get_if<TextureExport>(&entry))
            {
                const auto id = texture->resource;
                auto *object = Texture(id);
                const auto &resource = m_Plan.textures[m_Textures[id.Identity().index]];
                if (resource.transientSlot)
                {
                    uses.Add(*object, texture->range, OutputState(*texture));
                }
                auto retained = RetainOutput(*texture, resource);
                const auto state = resource.importState ? resource.importState->final : OutputState(*texture);
                m_Outputs->emplace_back(GraphTextureOutput{*texture, retained ? RHI::TextureHandle{} :
                    RHI::TextureHandle(object), std::move(retained), state});
            }
            else
            {
                const auto &buffer = std::get<BufferExport>(entry);
                const auto id = buffer.resource;
                auto *object = Buffer(id);
                const auto &resource = m_Plan.buffers[m_Buffers[id.Identity().index]];
                if (resource.transientSlot)
                {
                    uses.Add(*object, OutputState(buffer));
                }
                auto retained = RetainOutput(buffer, resource);
                const auto state = resource.importState ? resource.importState->final : OutputState(buffer);
                m_Outputs->emplace_back(GraphBufferOutput{buffer, retained ? RHI::BufferHandle{} :
                    RHI::BufferHandle(object), std::move(retained), state});
            }
        }
        return uses.Commit(m_Recording, m_Diagnostics, m_Stage, {});
    }

    GraphFailure GraphExecutor::Record()
    try
    {
        for (std::size_t position = 0; position < m_Plan.passes.size(); ++position)
        {
            if (auto failure = ExecutePass(m_Plan.passes[position], position))
            {
                return failure;
            }
        }
        return Handoff();
    }
    catch (const std::bad_alloc &)
    {
        return Error(GraphErrorCode::OutOfStorage, m_Stage, {}, "Could not prepare execution storage");
    }
    catch (const std::length_error &)
    {
        return Error(GraphErrorCode::OutOfStorage, m_Stage, {}, "Execution storage size is not representable");
    }
    catch (const std::exception &error)
    {
        if (!m_Diagnostics.IsClear())
        {
            return Error(GraphErrorCode::ExecutionFailed, m_Stage, {}, "RHI reported Error/Fatal; see the device diagnostic");
        }
        return Error(GraphErrorCode::ExecutionFailed, m_Stage, {}, error.what());
    }
    catch (...)
    {
        return Error(GraphErrorCode::ExecutionFailed, m_Stage, {}, "Pass preparation threw");
    }

    template <typename Planned, typename Declared>
    bool GraphExecutor::RetainedImportsMatch(const Planned &planned, const Declared &declared,
                                             RHI::Device &device, GraphPoolReservation *reservation) noexcept
    {
        return std::ranges::all_of(planned, [&](const auto &resource)
        {
            const auto &definition = declared[resource.resource.Identity().index];
            const auto &entry = definition.retained.m_Entry;
            if (!entry)
            {
                return true;
            }
            const auto *accepted = entry->AcceptedUse();
            if (!accepted || entry->device.Get() != &device ||
                (reservation && &entry->owner != &reservation->m_Pool))
            {
                return false;
            }
            if (accepted == definition.importUse.get())
            {
                return true;
            }
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(resource)>, PlannedGraphTexture>)
            {
                const auto &pin = definition.retained.m_ContentPin;
                const auto &range = resource.range;
                // 图持有使只读池纹理保留原代. 若池项可原地更新, 即使当前引用只读也须有覆盖导入范围的快照.
                if (entry->mutableContent && (!pin || !Contains(*pin, range)))
                {
                    return false;
                }
                // 内容保护区分 aspect. RHI 状态按 mip/layer 核对本图声明会接入的单元.
                for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
                {
                    for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
                    {
                        if (TextureStateAt(*accepted, definition.desc, layer, mip) !=
                            TextureStateAt(*definition.importUse, definition.desc, layer, mip))
                        {
                            return false;
                        }
                    }
                }
                return true;
            }
            else
            {
                return false;
            }
        });
    }

    GraphResult<GraphExecutionOutputs> GraphExecutor::Run(
        const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
        RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics, GraphPoolReservation *reservation)
    {
        auto result = RunRecording(graph, plan, device, commands, diagnostics, reservation);
        if (reservation && reservation->m_Executed)
        {
            reservation->Release();
        }
        return result;
    }

    GraphResult<GraphExecutionOutputs> GraphExecutor::RunRecording(
        const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
        RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics, GraphPoolReservation *reservation)
    try
    {
        if (!graph.m_Sealed || graph.m_Active || graph.m_Generation != plan.graphGeneration)
        {
            return MakeGraphError(GraphErrorCode::InvalidPhase, "GraphExec.Preflight", {}, {},
                                  "A sealed idle graph matching the compiled generation is required");
        }
        if (!diagnostics.isClear || !diagnostics.IsClear())
        {
            return MakeGraphError(GraphErrorCode::InvalidRecording, "GraphExec.Preflight", {}, {},
                                  "A current clear device diagnostic scope is required");
        }
        if (reservation && (!reservation->m_Committed || reservation->m_Executed ||
            &reservation->m_Plan != &plan || &reservation->m_Pool.m_Device != &device))
        {
            return MakeGraphError(GraphErrorCode::InvalidPhase, "GraphExec.Preflight", {}, {},
                                  "Graph pool reservation belongs to a different plan or device or was consumed");
        }
        if (!RetainedImportsMatch(plan.textures, graph.Textures(), device, reservation) ||
            !RetainedImportsMatch(plan.buffers, graph.Buffers(), device, reservation))
        {
            return MakeGraphError(GraphErrorCode::StateMismatch, "GraphExec.Preflight", {}, {},
                                  "A retained import changed device, pool or entry state after declaration");
        }
        RenderGraph::Activity activity(graph.m_Active);
        // 先检查能从计划判定的出口冲突. 录制模式只在实际后端接管成功后改变.
        if (auto failure = CheckTransientExports(graph, plan))
        {
            return *failure;
        }
        if (auto failure = CheckRetainedWrites(graph, plan))
        {
            return *failure;
        }
        RHI::GraphRecording scope(commands, device);
        if (!scope)
        {
            return MakeGraphError(GraphErrorCode::InvalidRecording, "GraphExec.Preflight", {}, {},
                                  "An owning current Automatic Graphics recording with no pending barriers is required");
        }
        if (reservation)
        {
            reservation->m_Executed = true;
        }
        GraphExecutor executor(graph, plan, device, commands, scope, diagnostics, reservation);
        const auto failure = executor.Record();
        const bool restored = scope.Finish(!failure);
        // 先归还模式, 再决定是否交付. 收拢过程产生的诊断不覆盖已记录的 Pass 或图尾错误.
        if (failure)
        {
            return *failure;
        }
        if (!restored || !diagnostics.IsClear())
        {
            return MakeGraphError(GraphErrorCode::ExecutionFailed, "GraphExec.Exit", {}, {},
                                  "The recording became unavailable during execution");
        }
        return GraphExecutionOutputs(std::move(executor.m_Outputs));
    }
    catch (const std::bad_alloc &)
    {
        return MakeGraphError(GraphErrorCode::OutOfStorage, "GraphExec.Preflight", {}, {}, "Could not store execution plan");
    }
    catch (const std::length_error &)
    {
        return MakeGraphError(GraphErrorCode::OutOfStorage, "GraphExec.Preflight", {}, {},
                              "Execution mapping size is not representable");
    }
}

namespace Physara::Renderer
{
    GraphResult<GraphExecutionOutputs> ExecuteRenderGraph(
        const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device, RHI::CommandList &commands,
        GraphExecutionDiagnostics diagnostics, GraphPoolReservation *reservation)
    {
        return Detail::GraphExecutor::Run(graph, plan, device, commands, diagnostics, reservation);
    }
}
