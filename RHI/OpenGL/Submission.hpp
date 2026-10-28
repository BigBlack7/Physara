#pragma once

#include "Upload.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Physara::RHI::OpenGL
{
    enum class PacketPhase : std::uint8_t
    {
        Recording,
        Closed,
        Submitted
    };

    // tracker/lease 地址稳定至提交或 owner 退休. 无逻辑列表强引用, 避免另一层寿命环.
    struct UploadWorkPacket
    {
        explicit UploadWorkPacket(MessageCallback *callback, std::uint64_t identity, std::size_t minimumChunk);
        CommandListResourceStateTracker tracker;
        const std::uint64_t recordingId;
        const std::size_t minimumChunk;
        PacketPhase phase = PacketPhase::Recording;
        std::atomic<bool> orphaned{false};
        bool issuedWork = false;
        bool invalid = false;
        bool automaticBarriers = true;
        std::size_t writes = 0;
        std::size_t logicalBytes = 0;
        std::vector<UploadResourceLease> resources;
        std::vector<std::unique_ptr<UploadChunk>> chunks;
    };

    struct SubmissionRecord
    {
        GLsync sync = nullptr;
        std::uint64_t submissionId = 0;
        std::array<UploadWorkPacket *, UploadPacketLimit> packets{};
        std::size_t count = 0;
    };

    // 有界 FIFO 的记录在 Device 发布前分配, 发出 fence 后不再申请 CPU 内存.
    struct SubmissionState
    {
        std::array<SubmissionRecord, UploadPacketLimit> records{};
        std::size_t head = 0;
        std::size_t count = 0;
    };
}
