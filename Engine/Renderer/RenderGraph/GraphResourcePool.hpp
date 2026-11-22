#pragma once

/// @file
/// @brief 在同一设备的图之间管理完整 Texture/Buffer 物理对象和准入额度.
/// Runtime 串行推进退休和准入. 执行器首用物化, 对应 RHI 包提供真实状态和完成.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "GraphCompiler.hpp"

namespace Physara::Renderer
{
    /// @brief Texture 与 Buffer 共用一对额度. bytes 按完整存储描述计, objects 按物理槽计.
    struct GraphPoolCapacity
    {
        std::uint64_t bytes{};
        std::size_t objects{};
    };

    /// @brief used 包含已承诺但尚未创建的槽. 四类占用互斥, 其对象数和字节数分别合计为 used.
    /// reserved 包含准入预留和尚未取得票据的录制. inFlight 只计已接受且尚未观察到完成的使用.
    /// highWater 的两项分别记录历史峰值. hits 和 created 在执行器实际取得对象时累计.
    struct GraphPoolStatistics
    {
        GraphPoolCapacity limit, idleLimit, used, highWater, reserved, inFlight, retained, idle;
        std::uint64_t hits{}, created{}, evicted{}, rejected{};
    };

    class GraphResourcePool;

    /// @brief 保留单一计划全部存活槽的分配额度. 执行出口结清额度, 弃用时由析构归还.
    /// @pre 原计划及原池存活至本对象释放; 执行只消费一次.
    class GraphPoolReservation final
    {
    public:
        ~GraphPoolReservation();
        GraphPoolReservation(const GraphPoolReservation &) = delete;
        GraphPoolReservation &operator=(const GraphPoolReservation &) = delete;

    private:
        friend class GraphResourcePool;
        friend class Detail::GraphExecutor;
        GraphPoolReservation(GraphResourcePool &pool, const CompiledGraphPlan &plan);
        /// @brief 解除预留引用. 已登记的录制和输出各自保持物理对象的寿命.
        void Release() noexcept;

        GraphResourcePool &m_Pool;
        const CompiledGraphPlan &m_Plan;
        std::vector<std::shared_ptr<Detail::GraphPoolEntry>> m_Slots;
        bool m_Committed{};
        bool m_Executed{};
    };

    /// @brief 由渲染设备会话持有的图池. 所有方法在该设备 owner 线程串行调用.
    /// @note 只保存本池物理槽. 原有外部存储和宿主呈现各自保留原预算.
    class GraphResourcePool final
    {
    public:
        static constexpr GraphPoolCapacity DefaultLimit{768ull * 1024 * 1024, 256};
        static constexpr GraphPoolCapacity DefaultIdle{128ull * 1024 * 1024, 64};

        explicit GraphResourcePool(RHI::Device &device, GraphPoolCapacity limit = DefaultLimit,
                                   GraphPoolCapacity idleLimit = DefaultIdle);
        GraphResourcePool(const GraphResourcePool &) = delete;
        GraphResourcePool &operator=(const GraphResourcePool &) = delete;

        /// @brief 在 Open 前匹配空闲对象及新建额度. 准入失败时无本次预留, 不调用设备创建.
        /// @return 成功时取得限用于 plan 的预留. 硬超限或已有占用阻挡时返回含请求量和上限的诊断.
        [[nodiscard]] GraphResult<std::unique_ptr<GraphPoolReservation>> Admit(const CompiledGraphPlan &plan) noexcept;
        /// @brief 在现有 Device 回收推进后淘汰完成且无继续持有的对象, 遵守空闲保留上限.
        void Collect() noexcept;
        /// @brief 读取此刻的容量与累计操作数. 完成进度由设备推进, 查询本身不轮询 fence.
        [[nodiscard]] GraphPoolStatistics Statistics() const noexcept;

    private:
        friend class GraphPoolReservation;
        friend class Detail::GraphExecutor;
        void ReleaseUnused() noexcept;
        GraphError AdmissionFailure(GraphErrorCode code, GraphPoolCapacity demand) noexcept;

        RHI::Device &m_Device;
        GraphPoolStatistics m_Statistics;
        std::vector<std::shared_ptr<Detail::GraphPoolEntry>> m_Entries = std::vector<
            std::shared_ptr<Detail::GraphPoolEntry>>(0);
    };
}
