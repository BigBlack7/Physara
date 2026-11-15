#pragma once

/// @file
/// @brief 有界 CPU 作业和完成交接. owner 提交、取回、取消并负责收拢.
/// worker 借用的只是单次作业的输入, 产出独立结果.

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <Core/Identity.hpp>

namespace Physara::Core
{
    /// @brief 准入失败类别, Full 可在 owner 取走完成或移除排队任务后重试.
    enum class JobRejection
    {
        Full,
        Stopped,
        Exhausted
    };

    /// @brief 固定槽既存排队输入, 也存等待 owner 的结果. 运行中同样保留槽.
    /// @note 除 worker 的内部操作外, 所有接口由同一个 owner 线程调用.
    /// 队列不判断业务版本或是否发布, 取消后的完成包仍由业务 owner 裁决.
    /// @tparam Input 背景任务独立拥有的输入类型.
    /// @tparam Output 自持结果, 必须可无异常地移交到预留槽.
    template <typename Input, typename Output>
    class JobQueue final
    {
        static_assert(std::is_nothrow_move_constructible_v<Output>);
        struct JobTag;

    public:
        using Id = Identity<JobTag>;
        /// @brief worker 的无异常处理入口. 输入仅在调用期间有效, 结果不得借用该输入的存储.
        using Process = Output (*)(const Input &, std::stop_token) noexcept;

        /// @brief 创建有限数量的线程. 线程启动失败时先收拢已启动的线程再传播异常.
        /// @pre 0 < workerCount <= capacity, capacity 可由 Id 表示, process 有效.
        JobQueue(std::size_t capacity, std::size_t workerCount, Process process)
            : m_Slots(capacity), m_Process(process)
        {
            assert(capacity <= std::numeric_limits<std::uint32_t>::max());
            assert(workerCount != 0 && workerCount <= capacity && process != nullptr);
            m_Workers.reserve(workerCount);
            try
            {
                for (std::size_t i = 0; i < workerCount; ++i)
                {
                    m_Workers.emplace_back([this] { Work(); });
                }
            }
            catch (...)
            {
                Shutdown();
                throw;
            }
        }

        JobQueue(const JobQueue &) = delete;
        JobQueue &operator=(const JobQueue &) = delete;
        ~JobQueue() { Shutdown(); }

        /// @brief 尝试取得一个作业和完成共用的槽, 满时立即返回.
        /// @return 成功后的槽身份, 或拒绝原因. 异常发生时未接收新工作.
        /// @note input 的所有权在调用时移交, 拒绝也会释放输入; 重试须重新提供拥有型输入.
        [[nodiscard]] std::variant<Id, JobRejection> TrySubmit(std::unique_ptr<Input> input)
        {
            assert(input);
            Id id;
            {
                const std::lock_guard lock(m_Mutex);
                if (m_Stopping)
                {
                    return JobRejection::Stopped;
                }
                const auto slot = std::ranges::find(m_Slots, State::Free, &Slot::state);
                if (slot == m_Slots.end())
                {
                    return JobRejection::Full;
                }
                if (!AdvanceGeneration(m_LastGeneration))
                {
                    return JobRejection::Exhausted;
                }
                slot->stop.emplace();
                slot->input = std::move(input);
                slot->generation = m_LastGeneration;
                slot->state = State::Queued;
                id = {static_cast<std::uint32_t>(slot - m_Slots.begin()), slot->generation};
            }
            m_Ready.notify_one();
            return id;
        }

        /// @brief 取消匹配的作业. 未开始的输入释放槽, 运行中的输入收到合作取消.
        /// 已完成的结果仍由 owner 取走并裁决是否发布.
        void Cancel(Id id) noexcept
        {
            std::unique_ptr<Input> discarded;
            std::optional<std::stop_source> running;
            {
                const std::lock_guard lock(m_Mutex);
                if (!id.IsValid() || id.index >= m_Slots.size() || m_Slots[id.index].generation != id.generation)
                {
                    return;
                }
                auto &slot = m_Slots[id.index];
                if (slot.state == State::Queued)
                {
                    discarded = std::move(slot.input);
                    Retire(slot);
                }
                else if (slot.state == State::Running)
                {
                    running = slot.stop;
                }
            }
            // stop 回调及输入析构可能执行调用方的工作, 均在锁外发生.
            if (running)
            {
                running->request_stop();
            }
        }

        /// @brief 取走一个当前可用结果并退还作业额度, 无结果时立即返回.
        /// 完成按槽位置扫描, 不保证提交顺序; 返回值须能独立标识对应的业务请求.
        [[nodiscard]] std::optional<Output> TakeCompleted() noexcept
        {
            const std::lock_guard lock(m_Mutex);
            if (m_Stopping)
            {
                return std::nullopt;
            }
            const auto slot = std::ranges::find(m_Slots, State::Completed, &Slot::state);
            if (slot == m_Slots.end())
            {
                return std::nullopt;
            }
            std::optional<Output> output(std::in_place, std::move(*slot->output));
            Retire(*slot);
            return output;
        }

        /// @brief 停止准入和完成交付, 通知运行作业并等待退出, 最后释放未交付数据.
        /// @note 可重复调用. 已开始的底层调用须返回后才能完成 join.
        void Shutdown() noexcept
        {
            {
                const std::lock_guard lock(m_Mutex);
                if (m_Stopping)
                {
                    return;
                }
                m_Stopping = true;
            }
            m_Ready.notify_all();
            for (std::size_t index = 0; index < m_Slots.size(); ++index)
            {
                // generation 只由 owner 分配. 设置停止后没有新提交或作业启动.
                Cancel({static_cast<std::uint32_t>(index), m_Slots[index].generation});
            }
            for (auto &worker : m_Workers)
            {
                worker.join();
            }
            m_Workers.clear();
            m_Slots.clear();
        }

    private:
        /// @brief 正常路径为 Free -> Queued -> Running -> Completed -> Free.
        /// Queued 可由取消直接退休; Running/Completed 保留额度直到交付或 Shutdown.
        enum class State
        {
            Free,
            Queued,
            Running,
            Completed
        };

        /// @brief 槽存储构造后地址固定, 所有可变内容受 m_Mutex 保护; generation 只由 owner 分配.
        struct Slot
        {
            State state = State::Free;
            std::uint64_t generation = 0;
            std::unique_ptr<Input> input;
            std::optional<Output> output;
            std::optional<std::stop_source> stop;
        };

        /// @brief 结束一个槽的当前占用. 退休前 owner 已取走仍需保留的对象.
        static void Retire(Slot &slot) noexcept
        {
            slot.output.reset();
            slot.stop.reset();
            slot.state = State::Free;
        }

        /// @brief 在锁内选择最早接收的待执行作业. 完成顺序由实际运行决定.
        [[nodiscard]] Slot *FindQueued() noexcept
        {
            Slot *oldest = nullptr;
            for (auto &slot : m_Slots)
            {
                if (slot.state == State::Queued && (!oldest || slot.generation < oldest->generation))
                {
                    oldest = &slot;
                }
            }
            return oldest;
        }

        /// @brief 移走输入, 锁外执行, 再写入原槽. 完成交付不依赖 owner 有空取回.
        void Work() noexcept
        {
            for (;;)
            {
                Slot *slot = nullptr;
                std::unique_ptr<Input> input;
                std::stop_token stop;
                {
                    std::unique_lock lock(m_Mutex);
                    m_Ready.wait(lock, [&]
                    {
                        return m_Stopping || (slot = FindQueued()) != nullptr;
                    });
                    if (m_Stopping)
                    {
                        return;
                    }
                    slot->state = State::Running;
                    input = std::move(slot->input);
                    stop = slot->stop->get_token();
                }

                auto result = m_Process(*input, stop);
                input.reset();
                {
                    // 即使刚刚收到取消, 仍交回自持结果; worker 不越权修改业务 owner 的状态.
                    const std::lock_guard lock(m_Mutex);
                    slot->output.emplace(std::move(result));
                    slot->state = State::Completed;
                }
            }
        }

        std::vector<Slot> m_Slots;
        std::vector<std::thread> m_Workers = std::vector<std::thread>(0);
        Process m_Process;
        std::mutex m_Mutex;
        std::condition_variable m_Ready;
        std::uint64_t m_LastGeneration = 0;
        bool m_Stopping = false;
    };
}
