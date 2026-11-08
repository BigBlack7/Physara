#pragma once

/// @file
/// @brief Device 独占的物理包与有界 fence FIFO, recording 身份和公开提交票据分开.

#include "Upload.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Physara::RHI::OpenGL
{
    /// @brief 物理包的录制/待提交/已接受阶段, Closed 不等于 GPU 完成, orphaned 是另一条交接事实.
    enum class PacketPhase : std::uint8_t
    {
        Recording,
        Closed,
        Submitted
    };

    /// @brief 持有 tracker, 资源 lease 和映射 chunk 的稳定物理包, 寿命跨逻辑列表与 GPU 完成.
    /// @note 无逻辑列表强引用; orphaned 交接后只由 owner 处理, 避免内部寿命环.
    /// Timer 结果与包完成分别推进, 销毁 tracker/lease 前先按 recording 身份收拢关联查询.
    struct UploadWorkPacket
    {
        /// @brief 准备可失败的 tracker/容器; Device 取得唯一 claim 后才发布本包.
        explicit UploadWorkPacket(MessageCallback *callback, std::uint64_t identity, std::size_t minimumChunk);
        CommandListResourceStateTracker tracker;
        const std::uint64_t recordingId;
        const std::size_t minimumChunk;
        PacketPhase phase = PacketPhase::Recording;
        // worker 最后释放列表时以 release 交接, owner 以 acquire 接收后才可读取/退休包; 交接后 worker 不再访问.
        std::atomic<bool> orphaned{false};
        bool issuedWork = false;
        bool invalid = false;
        bool automaticBarriers = true;
        // 图形快照中的指针只借用, 默认由 resources 保活; trackLiveness=false 的集合归调用方保活.
        // 区域标记只记录本 recording 的显式输入, 不从跨 recording 的 native 缓存推定已设置.
        GraphicsState graphics;
        bool graphicsSet = false;
        bool viewportSet = false;
        bool scissorSet = false;
        std::size_t writes = 0;
        std::size_t logicalBytes = 0;
        std::vector<UploadResourceLease> resources;
        std::vector<OpenGLTimerQuery *> timers; ///< 借用 resources 保活的 query, 配对与退休时核对 recording 身份.
        std::vector<std::unique_ptr<UploadChunk>> chunks;
    };

    /// @brief 一个真实 fence 覆盖的物理包组, submissionId=0 用于 orphan 或失败前缀, 不作为公开成功票据.
    struct SubmissionRecord
    {
        GLsync sync = nullptr;
        std::uint64_t submissionId = 0;
        std::array<UploadWorkPacket *, UploadPacketLimit> packets{};
        std::size_t count = 0;
    };

    /// @brief Device 发布前取得存储的有界 FIFO, 发出 fence 后不再为记录申请 CPU 内存.
    struct SubmissionState
    {
        std::array<SubmissionRecord, UploadPacketLimit> records{};
        std::size_t head = 0;
        std::size_t count = 0;
    };
}
