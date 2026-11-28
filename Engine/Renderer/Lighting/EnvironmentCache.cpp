#include "EnvironmentCache.hpp"

/// @file
/// @brief 协调主线程的产品请求和单个 CPU 烘焙作业. 环境按源和角色复用, DFG 单独发布.

#include <algorithm>
#include <exception>
#include <new>
#include <span>
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
                return {job.serial, BakeEnvironment(job.source->image, job.request.product == Product::Reflection,
                    job.request.product == Product::Sheen, job.existing, stop)};
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
                m_Failures[std::size_t(request.product)] = *error;
            }
        }
    }

    void EnvironmentCache::CancelActive() noexcept
    {
        m_Queue->Cancel(m_Active->job);
        m_Active.reset();
    }

    void EnvironmentCache::SelectEnvironment(const std::optional<EnvironmentKey> &key) noexcept
    {
        if (m_EnvironmentKey == key)
        {
            return;
        }
        // 换源时应先撤销旧序号, 否则队列中已经完成的旧环境仍能在本轮 Collect 中发布.
        if (m_Active && m_Active->request.key)
        {
            CancelActive();
        }
        m_EnvironmentKey = key;
        for (auto &failure : std::span(m_Failures).first(std::size_t(Product::Dfg)))
        {
            failure.reset();
        }
    }

    void EnvironmentCache::BeginFrame(const Asset::EnvironmentVersion &source) noexcept
    {
        m_Needs.fill(false);
        SelectEnvironment(source ? std::optional(EnvironmentKey{source->sourceFingerprint}) : std::nullopt);
        Collect();
    }

    void EnvironmentCache::EndFrame() noexcept
    {
        if (m_Active && !m_Needs[std::size_t(m_Active->request.product)])
        {
            CancelActive();
        }
    }

    std::variant<EnvironmentProducts, EnvironmentPreparationFailure> EnvironmentCache::Prepare(
        const Asset::EnvironmentVersion &source, bool lighting, bool sheen, bool needsDfg)
    {
        const auto key = source ? std::optional(EnvironmentKey{source->sourceFingerprint}) : std::nullopt;
        // 一路没有环境需求不代表同帧其它路也不需要. 实际换源仍先拒收旧作业.
        if (key)
        {
            SelectEnvironment(key);
        }
        Collect();

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
        const bool missingReflection = lighting && (!existing || !existing->lighting);
        const bool missingSheen = sheen && (!existing || !existing->sheen.front());
        const bool missingBase = source && !existing;
        const bool missingDfg = needsDfg && !m_Dfg;
        if (!missingBase && !missingReflection && !missingSheen && !missingDfg)
        {
            return EnvironmentProducts{std::move(existing), needsDfg ? m_Dfg : nullptr};
        }

        // 每个子产品完成后独立发布. Sky 可先取基础层, 普通照明可在 Charlie 等待时取已完成的 GGX/SH.
        const auto product = missingDfg ? Product::Dfg : missingBase ? Product::Base :
                             missingReflection ? Product::Reflection : Product::Sheen;
        if (const auto &failure = m_Failures[std::size_t(product)]; failure)
        {
            return EnvironmentPreparationFailure{false, *failure};
        }
        m_Needs[std::size_t(Product::Base)] |= missingBase;
        m_Needs[std::size_t(Product::Reflection)] |= missingReflection;
        m_Needs[std::size_t(Product::Sheen)] |= missingSheen;
        m_Needs[std::size_t(Product::Dfg)] |= missingDfg;
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
            const Request required{product == Product::Dfg ? std::nullopt : key, product};
            const auto submitted = m_Queue->TrySubmit(std::make_unique<Input>(serial, required,
                required.key ? source : nullptr, std::move(existing)));
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
        constexpr std::array Pending{"Baking the requested HDR environment base",
            "Baking the requested GGX/SH environment", "Baking the requested Charlie environment",
            "Preparing the shared DFG lookup table"};
        return EnvironmentPreparationFailure{true, Core::BoundedText(Pending[std::size_t(product)])};
    }
}
