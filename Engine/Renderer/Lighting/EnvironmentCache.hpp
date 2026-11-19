#pragma once

/// @file
/// @brief Renderer 持有的进程内 CPU 烘焙缓存. 只在主线程观察候选, worker 不接触 Device.

#include <optional>
#include <variant>

#include <Core/Jobs.hpp>

#include "EnvironmentBake.hpp"

namespace Physara::Renderer::Detail
{
    /// @brief 只使用实际源字节、设置和处理 profile 定位烘焙. 查询时的朝向、强度和曝光不参与.
    struct EnvironmentKey
    {
        Core::Sha256Digest source;
        std::uint32_t version = EnvironmentBakeVersion;
        std::uint32_t edge = EnvironmentEdge;
        std::uint32_t mipCount = EnvironmentMipCount;
        std::uint32_t dfgVersion = DfgBakeVersion;
        auto operator<=>(const EnvironmentKey &) const = default;
    };

    /// @brief 一次准备的完整 CPU 结果. 仅天空请求可只有 environment, 无环境的物理表面可只有 dfg.
    struct EnvironmentProducts
    {
        std::shared_ptr<const BakedEnvironment> environment;
        std::shared_ptr<const DfgTable> dfg;
    };

    /// @brief 当前请求尚未达到完整产品, pending 只表示仍可继续推进的准入或完成.
    struct EnvironmentPreparationFailure
    {
        bool pending;
        Core::BoundedText reason;
    };

    /// @brief 主线程按源键复用两个环境槽, 按固定 profile 保留一份独立 DFG.
    /// 单个 worker 顺序烘焙, 发布资格属于当前作业的 serial. 帧已取得的只读产品独立保活.
    class EnvironmentCache final
    {
    public:
        /// @brief 开始同一 Scene 的帧需求收集. 切换源时先撤销旧作业, 再收取完成结果.
        void BeginFrame(const Asset::EnvironmentVersion &source) noexcept;
        /// @brief 所有 View 均已表达需求后, 按活动作业的产品类型决定是否取消.
        /// @note 帧内各路累计缺少的环境或 DFG. 诊断路跳过该产品时, 其它路的需求仍有效.
        void EndFrame() noexcept;
        /// @brief 推进一路所需的完整产品. source 为空时只准备所需的共享 DFG.
        /// @return worker 启动失败时返回带原因的拒绝, 此时没有活动作业. 后续调用可重新尝试启动.
        /// @note 同源的其它 View 可以保留活动作业. 队列析构时停止并收拢 CPU worker.
        [[nodiscard]] std::variant<EnvironmentProducts, EnvironmentPreparationFailure> Prepare(
            const Asset::EnvironmentVersion &source, bool lighting, bool needsDfg);

    private:
        struct Request
        {
            std::optional<EnvironmentKey> key; ///< 空键选择本缓存内独立于环境源的 DFG 作业.
            bool lighting = false;
            bool operator==(const Request &) const = default;
        };
        struct Input
        {
            std::uint64_t serial;
            Request request;
            Asset::EnvironmentVersion source;
            std::shared_ptr<const CubeLevel> base;
        };
        struct Completion
        {
            std::uint64_t serial;
            std::variant<std::monostate, std::shared_ptr<const BakedEnvironment>, std::shared_ptr<const DfgTable>,
                         Core::BoundedText> product;
        };
        using WorkQueue = Core::JobQueue<Input, Completion>;
        struct Active
        {
            Request request;
            WorkQueue::Id job;
            std::uint64_t serial;
        };
        struct Slot
        {
            std::optional<EnvironmentKey> key;
            std::shared_ptr<const BakedEnvironment> product;
        };

        /// @brief worker 从自持输入得到完整产品, 取消的空值或自持失败原因, 并带回原序号.
        [[nodiscard]] static Completion Process(const Input &job, std::stop_token stop) noexcept;
        /// @brief owner 归还完成槽, 仅将仍匹配活动序号的结果转入缓存.
        void Collect() noexcept;
        /// @brief 撤销当前作业的发布资格, 完成包仍由队列回收.
        void CancelActive() noexcept;

        std::array<Slot, 2> m_Stored; ///< 最近使用及上一个源, 可复用已完成的基础层.
        std::shared_ptr<const DfgTable> m_Dfg;
        std::optional<Core::BoundedText> m_DfgFailure;
        std::optional<std::pair<Request, Core::BoundedText>> m_EnvironmentFailure;
        std::optional<Active> m_Active;
        std::uint64_t m_LastSerial = 0;
        std::unique_ptr<WorkQueue> m_Queue;
        bool m_NeedsEnvironment{};
        bool m_NeedsDfg{};
    };
}
