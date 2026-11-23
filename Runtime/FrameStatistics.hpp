#pragma once

/// @file
/// @brief 发布实际接受的场景计时, 最近编译图及设备池容量. 各自按产生事实的阶段更新.

#include <array>

#include <DeviceHost.hpp>
#include <Session/FrameStatistics.hpp>

namespace Physara::Runtime
{
    /// @brief 在宿主线程保有查询, 必须先于宿主关闭销毁. 正常调度不等待结果.
    class FrameStatistics final
    {
    public:
        /// @brief 从就绪宿主的 Device 创建至多 MaxPresentationSlots 个查询对, 按实际取得数量供后续录制使用.
        explicit FrameStatistics(Platform::DeviceHost &host);

        /// @brief 单次录制借用的池项. 只有 query 非空时 index 才标识池内句柄.
        struct TimerReservation
        {
            RHI::TimerQuery *query{};
            std::size_t index{};
            Editor::GpuIntervalState state = Editor::GpuIntervalState::Unavailable;
        };

        /// @brief 借用空闲或已读取的查询. 成功录制以 FinishTimer 交回, 起点拒绝后可重置再复用.
        [[nodiscard]] TimerReservation ReserveTimer() const noexcept;
        /// @brief 结束已录制的区间. source 为空时仅回收有效区间, 不发布失败图的数值.
        /// @pre 区间的两个时间戳均已录制, 列表已 Close 并取得可靠票据.
        void FinishTimer(TimerReservation timer, const Editor::SceneFrameIdentity *source) noexcept;
        /// @brief 无法安全复用时释放池引用. RHI 保留已登记到包中的资源直至退休.
        void FailTimer(TimerReservation timer) noexcept;
        /// @brief 在录制开始前轮询待处理区间, 就绪后读取秒数. 同轮可回收多组查询.
        /// @note latestGpu 只保留有效结果中提交票据最大的一组. 逐轮读取快照可能跳过已经就绪的较早区间.
        void Poll(Platform::DeviceHost &host);

        /// @brief 发布已接受场景的值快照. 之前票据的就绪 GPU 区间沿自己的身份保留.
        void Publish(Editor::SceneFrameStatistics frame) noexcept { m_Stats.latestScene = frame; }
        /// @brief 每次窗口取得重新记录首个容量原因. 同一次取得内重建安全图时保留主候选原因.
        void BeginGraphFrame() noexcept { m_Stats.graphPressure.reset(); }
        /// @brief 编译成功就登记候选. 其后才可为这张图补充原生创建数和宿主回执.
        void PublishGraph(Editor::GraphFrameSummary graph) noexcept { m_Stats.latestGraph = graph; }
        void PublishPool(Renderer::GraphPoolStatistics pool) noexcept { m_Stats.graphPool = pool; }
        /// @brief 复制本轮 owner 准备及最近一次 worker 回报. 场景统计另随实际接受的提交发布.
        void PublishShaders(Renderer::ShaderPreparationStatistics native, Shader::ReloadStatistics cpu) noexcept
        {
            m_Stats.shader = native;
            m_Stats.shaderCpu = cpu;
        }
        void ReportPoolPressure(const Core::BoundedText &reason) noexcept
        {
            if (!m_Stats.graphPressure)
            {
                m_Stats.graphPressure = reason;
            }
        }
        /// @brief ticket 也可属于失败前缀. accepted 表示本图完整执行且宿主确认了有效颜色.
        void GraphReceipt(std::uint64_t ticket, bool accepted) noexcept
        {
            m_Stats.latestGraph->submission = ticket;
            m_Stats.latestGraph->accepted = accepted;
        }
        void GraphStorageCreated(RHI::GraphicsOperationCounts native) noexcept
        {
            m_Stats.latestGraph->nativeTextures = native.textureStorages;
            m_Stats.latestGraph->nativeBuffers = native.bufferStorages;
        }
        [[nodiscard]] const Editor::FrameStatistics &Snapshot() const noexcept { return m_Stats; }

    private:
        /// @brief awaiting 跨帧保留一次待解析的区间. 只有产生正常场景输出的区间才附带 source.
        struct Interval
        {
            RHI::TimerQueryHandle query;
            bool awaiting{};
            std::optional<Editor::SceneFrameIdentity> source;
        };

        std::array<Interval, Platform::MaxPresentationSlots> m_Intervals;
        bool m_QueryFailed{};
        Editor::FrameStatistics m_Stats;
    };
}
