#include "GraphExecutor.hpp"

/// @file
/// @brief 按编译顺序物化并消费存活 Pass. 资源状态交接和已发工作留在同一次即时录制.

#include <cassert>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include <ImmediateGraphics.hpp>

#include "GraphExecutionStates.hpp"
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
            RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics);

    private:
        using Slot = std::variant<std::monostate, RHI::TextureHandle, RHI::BufferHandle>;
        static constexpr std::size_t Unused = (std::numeric_limits<std::size_t>::max)();

        GraphExecutor(const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
                      RHI::CommandList &commands, RHI::GraphRecording &recording, GraphExecutionDiagnostics diagnostics)
            : m_Graph(graph), m_Plan(plan), m_Device(device), m_Commands(commands), m_Recording(recording),
              m_Diagnostics(diagnostics),
              m_Slots(plan.transientSlots.size()), m_Textures(graph.Textures().size(), Unused),
              m_Buffers(graph.Buffers().size(), Unused), m_TextureImports(plan.textures.size(), 0),
              m_BufferImports(plan.buffers.size(), 0), m_Outputs(std::make_unique<GraphExecutionOutputList>(0))
        {
            // 输出持有者和容量在发出图命令之前备妥, 图尾只填入句柄并移交持有者.
            m_Outputs->reserve(plan.exports.size());
            // 计划的存活表按声明索引递增但省略了裁剪资源, 只填本次会被访问的路由.
            for (std::size_t i = 0; i < plan.textures.size(); ++i)
            {
                m_Textures[plan.textures[i].resource.identity.index] = i;
            }
            for (std::size_t i = 0; i < plan.buffers.size(); ++i)
            {
                m_Buffers[plan.buffers[i].resource.identity.index] = i;
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
        const auto index = id.identity.index;
        assert(m_Textures[index] != Unused);
        const auto slot = m_Plan.textures[m_Textures[index]].transientSlot;
        return slot ? std::get<RHI::TextureHandle>(m_Slots[*slot]).Get() : m_Graph.Textures()[index].imported.Get();
    }

    RHI::Buffer *GraphExecutor::Buffer(BufferId id) const noexcept
    {
        const auto index = id.identity.index;
        assert(m_Buffers[index] != Unused);
        const auto slot = m_Plan.buffers[m_Buffers[index]].transientSlot;
        return slot ? std::get<RHI::BufferHandle>(m_Slots[*slot]).Get() : m_Graph.Buffers()[index].imported.Get();
    }

    GraphFailure GraphExecutor::TrackImport(RHI::Texture &texture, const PlannedGraphTexture &resource)
    {
        const auto entry = resource.importState->entry;
        const auto &range = resource.range;
        const auto &name = texture.GetDesc().debugName;
        // 已知状态来自同一包, 或后端已建立的首次 Common/keepInitialState 约定.
        bool needsTracking = false;
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
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
                needsTracking |= prior == ResourceStates::Unknown;
            }
        }
        if (needsTracking)
        {
            // 全范围已核对一致, 重记已知等值不会改变状态, 并可用一次请求接管未知部分.
            m_Recording.BeginTracking(texture, RhiSubresources(range), entry);
            if (!m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, name, "BeginTrackingTextureState failed");
            }
        }
        return std::nullopt;
    }

    GraphFailure GraphExecutor::TrackImport(RHI::Buffer &buffer, const PlannedGraphBuffer &resource)
    {
        const auto entry = resource.importState->entry;
        const auto &desc = buffer.GetDesc();
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

    GraphFailure GraphExecutor::Materialize(TextureId id, std::size_t position)
    {
        const auto route = m_Textures[id.identity.index];
        const auto &resource = m_Plan.textures[route];
        if (resource.firstUse != position)
        {
            return std::nullopt;
        }
        const auto &declared = m_Graph.Textures()[id.identity.index];
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
            auto texture = m_Device.CreateTexture(desc);
            if (!texture || !m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName, "CreateTexture failed",
                             id.version);
            }
            if (!desc.keepInitialState)
            {
                // 为无自动初态协议的新存储登记 Common. keepInitialState 由 tracker 处理首次和后继录制.
                m_Recording.BeginTracking(*texture.Get(), RhiSubresources(resource.range), ResourceStates::Common);
                if (!m_Diagnostics.IsClear())
                {
                    return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName,
                                 "BeginTrackingTextureState failed", id.version);
                }
            }
            slot = std::move(texture);
        }
        return std::nullopt;
    }

    GraphFailure GraphExecutor::Materialize(BufferId id, std::size_t position)
    {
        const auto route = m_Buffers[id.identity.index];
        const auto &resource = m_Plan.buffers[route];
        if (resource.firstUse != position)
        {
            return std::nullopt;
        }
        const auto &declared = m_Graph.Buffers()[id.identity.index];
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
            auto buffer = m_Device.CreateBuffer(desc);
            if (!buffer || !m_Diagnostics.IsClear())
            {
                return Error(GraphErrorCode::ExecutionFailed, m_Stage, declared.desc.debugName, "CreateBuffer failed",
                             id.version);
            }
            if (!desc.keepInitialState)
            {
                const auto entry = desc.initialState == ResourceStates::Unknown ? ResourceStates::Common : desc.initialState;
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
            const auto &name = m_Graph.Textures()[id.identity.index].desc.debugName;
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
                textures.push_back({object, RhiSubresources(access.range), access.range.aspect, state});
            }
            else
            {
                const auto &buffer = std::get<BufferUse>(planned.declaration);
                const auto &access = buffer.access;
                auto *object = Buffer(access.resource);
                uses.Add(*object, *planned.state);
                // 行范围随本 Pass 授权. 结构化表另携带布局要求的绑定范围, 供后端核对预建集合.
                buffers.push_back({object, access.range, *planned.state, buffer.indexedBinding});
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
        auto failure = m_Graph.InvokePreparedPass(resources, &m_Commands);
        // 下一个 Pass 从自己的准备阶段开始. 上个回调的访问范围到这里失效, 已发工作的引用仍随包保留.
        m_Recording.RevokeUses();
        return failure;
    }

    GraphFailure GraphExecutor::Handoff()
    {
        m_CurrentPass = {};
        m_Stage = "GraphExec.Handoff";
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
                if (m_Plan.textures[m_Textures[id.identity.index]].transientSlot)
                {
                    uses.Add(*object, texture->range, OutputState(*texture));
                }
                m_Outputs->emplace_back(GraphTextureOutput{*texture, RHI::TextureHandle(object)});
            }
            else
            {
                const auto &buffer = std::get<BufferExport>(entry);
                const auto id = buffer.resource;
                auto *object = Buffer(id);
                if (m_Plan.buffers[m_Buffers[id.identity.index]].transientSlot)
                {
                    uses.Add(*object, OutputState(buffer));
                }
                m_Outputs->emplace_back(GraphBufferOutput{buffer, RHI::BufferHandle(object)});
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

    GraphResult<GraphExecutionOutputs> GraphExecutor::Run(
        const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device,
        RHI::CommandList &commands, GraphExecutionDiagnostics diagnostics)
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
        RenderGraph::Activity activity(graph.m_Active);
        // 先检查能从计划判定的出口冲突. 录制模式只在实际后端接管成功后改变.
        if (auto failure = CheckTransientExports(graph, plan))
        {
            return *failure;
        }
        RHI::GraphRecording scope(commands, device);
        if (!scope)
        {
            return MakeGraphError(GraphErrorCode::InvalidRecording, "GraphExec.Preflight", {}, {},
                                  "An owning current Automatic Graphics recording with no pending barriers is required");
        }
        GraphExecutor executor(graph, plan, device, commands, scope, diagnostics);
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
        GraphExecutionDiagnostics diagnostics)
    {
        return Detail::GraphExecutor::Run(graph, plan, device, commands, diagnostics);
    }
}
