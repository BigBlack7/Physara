#include "EnvironmentCache.hpp"

/// @file
/// @brief 协调主线程的产品请求和单个 CPU 烘焙作业. 环境按源和角色复用, DFG 单独发布.

#include <algorithm>
#include <exception>
#include <new>
#include <system_error>
#include <utility>

namespace Physara::Renderer::Detail
{
    EnvironmentCache::Completion EnvironmentCache::Process(const Input &job, std::stop_token stop) noexcept
    {
        try
        {
            if (job.source)
            {
                return {job.serial, BakeEnvironment(job.source->image, job.request.lighting, job.base, stop)};
            }
            return {job.serial, BakeDfg(stop)};
        }
        catch (const Core::BoundedText &error)
        {
            return {job.serial, error};
        }
        catch (const std::bad_alloc &)
        {
            return {job.serial, Core::BoundedText("Environment baking exceeds available memory")};
        }
        catch (const std::exception &error)
        {
            return {job.serial, Core::BoundedText(error.what())};
        }
    }

    // 先回收队列槽, 只发布仍拥有当前 serial 的完成. 同源的照明产品可共享原基础层.
    void EnvironmentCache::Collect() noexcept
    {
        if (!m_Queue)
        {
            return;
        }
        while (auto done = m_Queue->TakeCompleted())
        {
            if (!m_Active || done->serial != m_Active->serial)
            {
                continue;
            }
            const auto request = m_Active->request;
            m_Active.reset();
            if (auto *environment = std::get_if<std::shared_ptr<const BakedEnvironment>>(&done->product))
            {
                if (*environment)
                {
                    if (m_Stored[0].key != request.key)
                    {
                        m_Stored[1] = std::move(m_Stored[0]);
                    }
                    m_Stored[0] = {request.key, std::move(*environment)};
                }
            }
            else if (auto *dfg = std::get_if<std::shared_ptr<const DfgTable>>(&done->product))
            {
                m_Dfg = std::move(*dfg);
            }
            else if (const auto *error = std::get_if<Core::BoundedText>(&done->product))
            {
                if (request.key)
                {
                    m_EnvironmentFailure.emplace(request, *error);
                }
                else
                {
                    m_DfgFailure = *error;
                }
            }
        }
    }

    std::variant<EnvironmentProducts, EnvironmentPreparationFailure> EnvironmentCache::Prepare(
        const Asset::EnvironmentVersion &source, bool lighting, bool needsDfg)
    {
        const auto key = source ? std::optional(EnvironmentKey{source->sourceFingerprint}) : std::nullopt;
        // 换源或撤去照明时先撤销旧作业的发布资格, 随后才取可能已经到达的完成包.
        if (m_Active && m_Active->request.key &&
            (m_Active->request.key != key || (m_Active->request.lighting && !lighting)))
        {
            m_Queue->Cancel(m_Active->job);
            m_Active.reset();
        }
        Collect();
        if (m_EnvironmentFailure && m_EnvironmentFailure->first.key != key)
        {
            m_EnvironmentFailure.reset();
        }

        std::shared_ptr<const BakedEnvironment> existing;
        if (key)
        {
            const auto stored = std::ranges::find(m_Stored, key, &Slot::key);
            if (stored != m_Stored.end())
            {
                std::swap(*stored, m_Stored[0]);
                existing = m_Stored[0].product;
            }
        }
        const bool missingEnvironment = source && (!existing || (lighting && !existing->lighting));
        const bool missingDfg = needsDfg && !m_Dfg;
        if (!missingEnvironment && !missingDfg)
        {
            return EnvironmentProducts{std::move(existing), needsDfg ? m_Dfg : nullptr};
        }

        // 缺两种产品时先做可跨场景使用的 DFG. 同源从天空升级到照明可以沿用基础层.
        const Request required = missingDfg ? Request{} : Request{key, lighting};
        if (missingDfg && m_DfgFailure)
        {
            return EnvironmentPreparationFailure{false, *m_DfgFailure};
        }
        if (missingEnvironment && m_EnvironmentFailure && m_EnvironmentFailure->first == required)
        {
            return EnvironmentPreparationFailure{false, m_EnvironmentFailure->second};
        }
        if (!m_Active)
        {
            auto serial = m_LastSerial;
            if (!Core::AdvanceGeneration(serial))
            {
                return EnvironmentPreparationFailure{false, Core::BoundedText("Environment bake identity exhausted")};
            }
            if (!m_Queue)
            {
                try
                {
                    m_Queue = std::make_unique<WorkQueue>(2, 1, &Process);
                }
                catch (const std::system_error &error)
                {
                    // JobQueue 构造失败已收拢部分启动的 worker, 此处仍没有本次活动作业.
                    Core::BoundedText reason("Cannot start the environment bake worker: ");
                    reason.Append(error.what());
                    return EnvironmentPreparationFailure{false, reason};
                }
            }
            const auto submitted = m_Queue->TrySubmit(std::make_unique<Input>(serial, required,
                required.key ? source : nullptr, existing ? existing->base : nullptr));
            if (const auto *id = std::get_if<WorkQueue::Id>(&submitted))
            {
                m_LastSerial = serial;
                m_Active = Active{required, *id, serial};
            }
            else if (std::get<Core::JobRejection>(submitted) != Core::JobRejection::Full)
            {
                return EnvironmentPreparationFailure{false, Core::BoundedText("Environment bake queue unavailable")};
            }
        }
        return EnvironmentPreparationFailure{true, Core::BoundedText(
            missingDfg ? "Preparing the shared DFG lookup table" : "Baking the requested HDR environment")};
    }
}
