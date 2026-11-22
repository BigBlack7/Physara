#include "GraphResourcePool.hpp"

/// @file
/// @brief 在设备会话内为存活物理槽准入和淘汰存储, 并为跨帧出口保留录制观察.
/// 准入只改 CPU 账本. 首次物化由执行器负责, Close 状态与完成事实由实际 Graphics 包提供.

#include <algorithm>
#include <cassert>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "GraphCompilerInternal.hpp"
#include "GraphPoolInternal.hpp"
#include "GraphRanges.hpp"

namespace Physara::Renderer::Detail
{
    GraphMutableTextureContent::GraphMutableTextureContent(const RHI::TextureDesc &desc)
        : m_Mips(desc.mipLevels), m_Current(std::size_t(desc.arraySize) * m_Mips),
          m_Prior(m_Current.size()), m_Readers(0)
    {
    }

    void GraphMutableTextureContent::BeginRecording(bool restorePrior) noexcept
    {
        if (restorePrior)
        {
            std::ranges::copy(m_Prior, m_Current.begin());
        }
        else
        {
            std::ranges::copy(m_Current, m_Prior.begin());
        }
    }

    void GraphMutableTextureContent::SetDefined(const TextureRange &range, bool defined) noexcept
    {
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                auto &cell = m_Current[Cell(layer, mip)];
                cell = defined ? cell | range.aspect : cell & ~range.aspect;
            }
        }
    }

    bool GraphMutableTextureContent::Covers(const TextureRange &range, bool usePrior) const noexcept
    {
        const auto &coverage = usePrior ? m_Prior : m_Current;
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                if ((coverage[Cell(layer, mip)] & range.aspect) != range.aspect)
                {
                    return false;
                }
            }
        }
        return true;
    }

    std::vector<TextureRange> GraphMutableTextureContent::DefinedRanges(const TextureRange &range, bool usePrior) const
    {
        // 在同一 mip 内只合并分量集合相同的连续层. 遇到未定义层会留下空洞.
        const auto &coverage = usePrior ? m_Prior : m_Current;
        std::vector<TextureRange> ranges(0);
        for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
        {
            for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
            {
                const auto aspect = coverage[Cell(layer, mip)] & range.aspect;
                if (aspect == TextureAspect{})
                {
                    continue;
                }
                if (!ranges.empty() && ranges.back().firstMip == mip && ranges.back().aspect == aspect &&
                    ranges.back().firstLayer + ranges.back().layerCount == layer)
                {
                    ++ranges.back().layerCount;
                }
                else
                {
                    ranges.push_back({mip, 1, layer, 1, aspect});
                }
            }
        }
        return ranges;
    }

    std::shared_ptr<const TextureRange> GraphMutableTextureContent::Pin(const TextureRange &range)
    {
        std::erase_if(m_Readers, [](const auto &reader) { return reader.expired(); });
        auto pin = std::make_shared<const TextureRange>(range);
        m_Readers.emplace_back(pin);
        return pin;
    }

    bool GraphMutableTextureContent::HasReader(const TextureRange &range) const noexcept
    {
        return std::ranges::any_of(m_Readers, [&](const auto &reader)
        {
            const auto pin = reader.lock();
            return pin && Intersect(*pin, range).has_value();
        });
    }

    std::shared_ptr<const RHI::GraphicsResourceUse> GraphPoolEntry::ShareAcceptedUse() const noexcept
    {
        if (use && !use->unused)
        {
            return use->ticket ? use : nullptr;
        }
        return prior;
    }

    const RHI::GraphicsResourceUse *GraphPoolEntry::AcceptedUse() const noexcept
    {
        if (use && !use->unused)
        {
            return use->ticket ? use.get() : nullptr;
        }
        return prior.get();
    }

    bool GraphPoolEntry::Pending() const noexcept
    {
        if (use && !use->unused)
        {
            return !use->complete;
        }
        return prior && !prior->complete;
    }

    bool GraphPoolEntry::AwaitingSubmission() const noexcept
    {
        return use && !use->unused && use->ticket == 0;
    }

    bool GraphPoolEntry::Observe(RHI::CommandList &commands)
    {
        // 同一包内可多次遇到该对象. 共用观察才能使图尾以后的合法访问也反映在本次 Close 状态中.
        auto observation = RHI::ObserveGraphicsResource(commands, *device.Get(), *object.Get(), AcceptedUse());
        if (!observation)
        {
            return false;
        }
        if (observation == use)
        {
            return true;
        }
        if (use && !use->unused)
        {
            if (use->ticket == 0)
            {
                return false;
            }
            prior = use;
        }
        if (mutableContent)
        {
            mutableContent->BeginRecording(use && use->unused);
        }
        use = std::move(observation);
        return true;
    }
}

namespace Physara::Renderer
{
    using Detail::GraphPoolEntry;
    using EntryPtr = std::shared_ptr<GraphPoolEntry>;

    // 依据实际 owner 和观察结果划分容量. 仍有额外 RHI 引用的对象不能被覆盖或淘汰.
    static bool Idle(const EntryPtr &entry) noexcept
    {
        return entry.use_count() == 1 && !entry->reserved && entry->object && !entry->Pending() &&
            entry->object->GetRefCount() == 1;
    }

    // 压力诊断只扣除能随 RHI 包消失的引用. 当前准入挑中的空闲槽仍计入其需求.
    static bool HeldBeyondRecording(const EntryPtr &entry) noexcept
    {
        if (entry.use_count() > (entry->reserved ? 2 : 1))
        {
            return true;
        }
        if (!entry->object || entry->object->GetRefCount() == 1)
        {
            return false;
        }
        const auto *use = entry->use.get();
        return !use || !use->hasExternalOwners || use->hasExternalOwners(*entry->object.Get(), 1);
    }

    // 完成的未知末态只允许淘汰. 未物化对象和无实发工作的新对象由取消路径释放.
    static bool KnownState(const GraphPoolEntry &entry) noexcept
    {
        const auto *use = entry.AcceptedUse();
        return use && !use->states.empty() &&
            std::ranges::find(use->states, RHI::ResourceStates::Unknown) == use->states.end();
    }

    // 先按对象类型选描述, 再沿用编译器的存储兼容条件.
    static bool Compatible(const GraphPoolEntry &entry, const PlannedGraphSlot &slot) noexcept
    {
        return std::visit([&](const auto &desc)
        {
            using Desc = std::decay_t<decltype(desc)>;
            const auto *existing = std::get_if<Desc>(&entry.desc);
            return existing && Detail::CompatibleStorage(*existing, desc);
        }, slot.desc);
    }

    static void Add(GraphPoolCapacity &capacity, const GraphPoolEntry &entry) noexcept
    {
        capacity.bytes += entry.bytes;
        ++capacity.objects;
    }

    // 两个维度都须有余量. 先确认现有占用可扣除, 再以剩余容量容纳新增需求.
    static bool Fits(GraphPoolCapacity demand, GraphPoolCapacity used, GraphPoolCapacity limit) noexcept
    {
        return used.bytes <= limit.bytes && used.objects <= limit.objects &&
            demand.bytes <= limit.bytes - used.bytes && demand.objects <= limit.objects - used.objects;
    }

    // 只在已解除租用的候选中选择最早一次实际使用. 同票据按现有表序确定.
    static auto OldestIdle(std::vector<EntryPtr> &entries) noexcept
    {
        auto oldest = entries.end();
        auto ticket = (std::numeric_limits<std::uint64_t>::max)();
        for (auto it = entries.begin(); it != entries.end(); ++it)
        {
            if (Idle(*it))
            {
                const auto *use = (*it)->AcceptedUse();
                const auto ordinal = use ? use->ticket : 0;
                if (oldest == entries.end() || ordinal < ticket)
                {
                    oldest = it;
                    ticket = ordinal;
                }
            }
        }
        return oldest;
    }

    GraphPoolReservation::GraphPoolReservation(GraphResourcePool &pool, const CompiledGraphPlan &plan)
        : m_Pool(pool), m_Plan(plan), m_Slots(plan.transientSlots.size())
    {
    }

    GraphPoolReservation::~GraphPoolReservation()
    {
        Release();
    }

    void GraphPoolReservation::Release() noexcept
    {
        if (std::exchange(m_Committed, false))
        {
            // 先解除此预留自己的强引用, 让未物化项可以从池账本移除. 已用项继续随观察和出口持有.
            for (const auto &entry : m_Slots)
            {
                entry->reserved = false;
            }
            m_Slots.clear();
            m_Pool.ReleaseUnused();
        }
    }

    GraphResourcePool::GraphResourcePool(RHI::Device &device, GraphPoolCapacity limit, GraphPoolCapacity idleLimit)
        : m_Device(device)
    {
        m_Statistics.limit = limit;
        m_Statistics.idleLimit = {(std::min)(limit.bytes, idleLimit.bytes),
                                  (std::min)(limit.objects, idleLimit.objects)};
    }

    void GraphResourcePool::ReleaseUnused() noexcept
    {
        // 到首用之前仅有描述和额度, 归还时没有 Texture/Buffer 需要等待设备退休.
        std::erase_if(m_Entries, [](const auto &entry) { return !entry->reserved && !entry->object; });
    }

    GraphError GraphResourcePool::AdmissionFailure(GraphErrorCode code, GraphPoolCapacity demand) noexcept
    {
        ++m_Statistics.rejected;
        Core::BoundedText reason(code == GraphErrorCode::PoolBudgetExceeded ? "Graph pool hard limit" :
            code == GraphErrorCode::PoolRecording ? "Graph pool capacity held by a reservation or unsubmitted recording" :
            code == GraphErrorCode::PoolInFlight ? "Graph pool waiting for Graphics completion" :
                                                 "Graph pool capacity held by other owners");
        reason.Append("; requested objects ");
        Detail::AppendDiagnosticNumber(reason, demand.objects);
        reason.Append(", bytes ");
        Detail::AppendDiagnosticNumber(reason, demand.bytes);
        reason.Append("; limit objects ");
        Detail::AppendDiagnosticNumber(reason, m_Statistics.limit.objects);
        reason.Append(", bytes ");
        Detail::AppendDiagnosticNumber(reason, m_Statistics.limit.bytes);
        return Detail::MakeGraphError(code, "GraphPool.Admit", {}, {}, reason.View());
    }

    GraphResult<std::unique_ptr<GraphPoolReservation>> GraphResourcePool::Admit(const CompiledGraphPlan &plan) noexcept
    try
    {
        // 先判断空池能否容纳整个计划. 之后只为未命中的槽追加额度, 已匹配对象已经计入现有占用.
        const GraphPoolCapacity demand{plan.transientBytes, plan.transientSlots.size()};
        if (!Fits(demand, {}, m_Statistics.limit))
        {
            return AdmissionFailure(GraphErrorCode::PoolBudgetExceeded, demand);
        }
        auto result = std::unique_ptr<GraphPoolReservation>(new GraphPoolReservation(*this, plan));
        GraphPoolCapacity missing;
        for (std::size_t slot = 0; slot < plan.transientSlots.size(); ++slot)
        {
            const auto &planned = plan.transientSlots[slot];
            const auto hit = std::ranges::find_if(m_Entries, [&](const auto &candidate)
            {
                return Idle(candidate) && Compatible(*candidate, planned) && KnownState(*candidate);
            });
            if (hit != m_Entries.end())
            {
                // 临时强引用也令后续槽无法从同一空闲项再命中.
                result->m_Slots[slot] = *hit;
            }
            else
            {
                missing.bytes += planned.bytes;
                ++missing.objects;
            }
        }
        GraphPoolCapacity protectedCapacity, used;
        for (const auto &entry : m_Entries)
        {
            Add(used, *entry);
            if (!Idle(entry))
            {
                Add(protectedCapacity, *entry);
            }
        }
        if (!Fits(missing, protectedCapacity, m_Statistics.limit))
        {
            GraphPoolCapacity retainedCapacity, withoutInFlight;
            // 实际拒绝时才追查原生附件/绑定的强引用, 避免为正常准入扫描包依赖.
            for (const auto &entry : m_Entries)
            {
                if (!Idle(entry))
                {
                    if (HeldBeyondRecording(entry))
                    {
                        Add(retainedCapacity, *entry);
                        Add(withoutInFlight, *entry);
                    }
                    else if (entry->reserved || entry->AwaitingSubmission())
                    {
                        Add(withoutInFlight, *entry);
                    }
                }
            }
            const auto cause = !Fits(missing, retainedCapacity, m_Statistics.limit) ? GraphErrorCode::PoolRetained :
                Fits(missing, withoutInFlight, m_Statistics.limit) ? GraphErrorCode::PoolInFlight :
                    GraphErrorCode::PoolRecording;
            return AdmissionFailure(cause, demand);
        }

        // 实际表修改前备足 CPU 存储. 剩余操作只移动已持有值和释放合格空闲项.
        m_Entries.reserve(used.objects + (std::min)(missing.objects, m_Statistics.limit.objects - used.objects));
        for (std::size_t slot = 0; slot < plan.transientSlots.size(); ++slot)
        {
            if (!result->m_Slots[slot])
            {
                result->m_Slots[slot] = std::make_shared<GraphPoolEntry>(*this, m_Device, plan.transientSlots[slot]);
            }
        }
        while (!Fits(missing, used, m_Statistics.limit))
        {
            const auto oldest = OldestIdle(m_Entries);
            assert(oldest != m_Entries.end());
            used.bytes -= (*oldest)->bytes;
            --used.objects;
            m_Entries.erase(oldest);
            ++m_Statistics.evicted;
        }
        for (const auto &entry : result->m_Slots)
        {
            entry->reserved = true;
            if (!entry->object)
            {
                m_Entries.push_back(entry);
            }
        }
        m_Statistics.highWater.bytes = (std::max)(m_Statistics.highWater.bytes, used.bytes + missing.bytes);
        m_Statistics.highWater.objects = (std::max)(m_Statistics.highWater.objects, used.objects + missing.objects);
        result->m_Committed = true;
        return result;
    }
    catch (const std::bad_alloc &)
    {
        return Detail::MakeGraphError(GraphErrorCode::OutOfStorage, "GraphPool.Admit", {}, {},
                                      "Could not hold graph slot reservations");
    }
    catch (const std::length_error &)
    {
        return Detail::MakeGraphError(GraphErrorCode::OutOfStorage, "GraphPool.Admit", {}, {},
                                      "Graph slot reservation size is not representable");
    }

    void GraphResourcePool::Collect() noexcept
    {
        // 先移除可释放但末态未知的项, 再按最后使用票据将其它空闲存储压到两项保留上限内.
        auto &entries = m_Entries;
        const auto removed = std::erase_if(entries, [](const auto &entry)
        {
            return Idle(entry) && !KnownState(*entry);
        });
        m_Statistics.evicted += removed;
        GraphPoolCapacity idle;
        for (const auto &entry : entries)
        {
            if (Idle(entry))
            {
                Add(idle, *entry);
            }
        }
        while (!Fits({}, idle, m_Statistics.idleLimit))
        {
            const auto oldest = OldestIdle(entries);
            assert(oldest != entries.end());
            idle.bytes -= (*oldest)->bytes;
            --idle.objects;
            entries.erase(oldest);
            ++m_Statistics.evicted;
        }
    }

    GraphPoolStatistics GraphResourcePool::Statistics() const noexcept
    {
        // 未交接的包与未用预留同桶. 取得票据后计在途, 完成后再按剩余引用区分保留和空闲.
        auto stats = m_Statistics;
        for (const auto &entry : m_Entries)
        {
            Add(stats.used, *entry);
            if (entry->reserved || entry->AwaitingSubmission())
            {
                Add(stats.reserved, *entry);
            }
            else if (entry->Pending())
            {
                Add(stats.inFlight, *entry);
            }
            else if (Idle(entry))
            {
                Add(stats.idle, *entry);
            }
            else
            {
                Add(stats.retained, *entry);
            }
        }
        return stats;
    }

    bool GraphRetainedResource::Accepted(std::uint64_t ticket) const noexcept
    {
        return ticket != 0 && m_Produced && m_Produced->ticket == ticket && HasStates(*m_Produced);
    }

    bool GraphRetainedResource::Readable() const noexcept
    {
        const auto *use = m_Entry ? m_Entry->AcceptedUse() : nullptr;
        return use && m_Produced && Accepted(m_Produced->ticket) && HasStates(*use) &&
            (!m_Entry->mutableContent || m_ContentPin ||
             m_Entry->mutableContent->Covers(std::get<TextureRange>(m_Range), m_Entry->use->unused));
    }

    bool GraphRetainedResource::HasStates(const RHI::GraphicsResourceUse &use) const noexcept
    {
        // 只检查本出口覆盖的物理状态单元. 深度和模板共用状态, Buffer 的跨列表状态覆盖整对象.
        if (const auto *range = std::get_if<TextureRange>(&m_Range))
        {
            const auto &desc = Texture()->GetDesc();
            for (auto layer = range->firstLayer; layer < range->firstLayer + range->layerCount; ++layer)
            {
                for (auto mip = range->firstMip; mip < range->firstMip + range->mipCount; ++mip)
                {
                    if (Detail::TextureStateAt(use, desc, layer, mip) == RHI::ResourceStates::Unknown)
                    {
                        return false;
                    }
                }
            }
            return true;
        }
        return use.states.front() != RHI::ResourceStates::Unknown;
    }

    RHI::Texture *GraphRetainedResource::Texture() const noexcept
    {
        return m_Entry ? dynamic_cast<RHI::Texture *>(m_Entry->object.Get()) : nullptr;
    }

    RHI::Buffer *GraphRetainedResource::Buffer() const noexcept
    {
        return m_Entry ? dynamic_cast<RHI::Buffer *>(m_Entry->object.Get()) : nullptr;
    }

    RHI::Texture *GraphRetainedResource::ReadbackTexture(RHI::CommandList &commands, TextureRange range) const
    {
        auto *texture = Texture();
        const auto *allowed = std::get_if<TextureRange>(&m_Range);
        if (!texture || !allowed || m_Usage != ExportUsage::Readback || !m_Produced || !Accepted(m_Produced->ticket) ||
            !Detail::Contains(*allowed, range))
        {
            return nullptr;
        }
        // 同录制重复借用时保留当前 tracker; 新录制用最后一次已接受的物理状态.
        auto previous = m_Entry->ShareAcceptedUse();
        if (!previous)
        {
            previous = m_Entry->prior;
        }
        if (!previous)
        {
            return nullptr;
        }
        const auto &desc = texture->GetDesc();
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                if (Detail::TextureStateAt(*previous, desc, layer, mip) == RHI::ResourceStates::Unknown)
                {
                    return nullptr;
                }
            }
        }
        if (!m_Entry->Observe(commands))
        {
            return nullptr;
        }
        for (auto layer = range.firstLayer; layer < range.firstLayer + range.layerCount; ++layer)
        {
            for (auto mip = range.firstMip; mip < range.firstMip + range.mipCount; ++mip)
            {
                if (commands.GetTextureSubresourceState(texture, layer, mip) == RHI::ResourceStates::Unknown)
                {
                    commands.BeginTrackingTextureState(texture, {mip, 1, layer, 1},
                        Detail::TextureStateAt(*previous, desc, layer, mip));
                    if (commands.GetTextureSubresourceState(texture, layer, mip) == RHI::ResourceStates::Unknown)
                    {
                        return nullptr;
                    }
                }
            }
        }
        return texture;
    }

    RHI::Buffer *GraphRetainedResource::ReadbackBuffer(RHI::CommandList &commands, RHI::BufferRange range) const
    {
        auto *buffer = Buffer();
        const auto *allowed = std::get_if<RHI::BufferRange>(&m_Range);
        if (!buffer || !allowed || m_Usage != ExportUsage::Readback || !m_Produced || !Accepted(m_Produced->ticket) ||
            !Detail::Contains(*allowed, range))
        {
            return nullptr;
        }
        // 字节区间限定本次复制. 跟踪入口按整个 Buffer 登记, 已在本录制建立的状态继续使用.
        auto previous = m_Entry->ShareAcceptedUse();
        if (!previous)
        {
            previous = m_Entry->prior;
        }
        if (!previous || previous->states.front() == RHI::ResourceStates::Unknown || !m_Entry->Observe(commands))
        {
            return nullptr;
        }
        if (commands.GetBufferState(buffer) == RHI::ResourceStates::Unknown)
        {
            commands.BeginTrackingBufferState(buffer, previous->states.front());
            if (commands.GetBufferState(buffer) == RHI::ResourceStates::Unknown)
            {
                return nullptr;
            }
        }
        return buffer;
    }
}
