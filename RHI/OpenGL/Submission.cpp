#include "Submission.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <limits>

#include "CommandList.hpp"

namespace Physara::RHI::OpenGL
{
    UploadWorkPacket::UploadWorkPacket(MessageCallback *callback, std::uint64_t identity, std::size_t minimum)
        : tracker(callback), recordingId(identity), minimumChunk(minimum), resources(0), chunks(0)
    {
    }

    bool OpenGLDevice::CheckCompletionFence(GLsync sync, const char *operation) noexcept
    {
        if (!CheckNative(operation))
        {
            return false;
        }
        const bool valid = sync != nullptr && glIsSync(sync) == GL_TRUE;
        if (!CheckNative(operation))
        {
            return false;
        }
        if (!valid)
        {
            Report(operation, "NativeFailure: no valid completion fence");
        }
        return valid;
    }

    uint64_t OpenGLDevice::ExecuteCommandLists(CommandList *const *lists, size_t count, CommandQueue queue)
    {
        if (count == 0)
        {
            return 0;
        }
        if (!RequireContext("ExecuteCommandLists"))
        {
            return 0;
        }
        if (queue != CommandQueue::Graphics || lists == nullptr || count > UploadPacketLimit ||
            m_ActiveRecording.load() != 0 || m_LastSubmissionId == std::numeric_limits<std::uint64_t>::max() ||
            m_Submissions->count == UploadPacketLimit)
        {
            Report("ExecuteCommandLists", "InvalidArgument: batch, queue, active recording or submission capacity");
            return 0;
        }
        std::array<OpenGLCommandList *, UploadPacketLimit> nativeLists{};
        for (std::size_t index = 0; index < count; ++index)
        {
            auto *list = dynamic_cast<OpenGLCommandList *>(lists[index]);
            if (list == nullptr || list->m_Device.Get() != this || list->m_Packet == nullptr ||
                list->m_Packet->phase != PacketPhase::Closed || list->m_Packet->invalid ||
                list->m_Packet->tracker.HasErrors())
            {
                Report("ExecuteCommandLists", "InvalidState: batch contains foreign, invalid or consumed list");
                return 0;
            }
            for (std::size_t previous = 0; previous < index; ++previous)
            {
                if (nativeLists[previous] == list)
                {
                    Report("ExecuteCommandLists", "InvalidArgument: duplicate list");
                    return 0;
                }
            }
            nativeLists[index] = list;
        }
        if (!BeginNative("ExecuteCommandLists"))
        {
            return 0;
        }
        auto &record = m_Submissions->records[(m_Submissions->head + m_Submissions->count) % UploadPacketLimit];
        record = {};
        record.count = count;
        for (std::size_t index = 0; index < count; ++index)
        {
            record.packets[index] = nativeLists[index]->m_Packet;
        }
        record.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        // 即使 flush 失败也保存真实 sync 供退出处理, 不把它发布为正常 Pending 票据.
        ++m_Submissions->count;
        if (!CheckCompletionFence(record.sync, "ExecuteCommandLists.Fence"))
        {
            m_WorkFailed = true;
            m_Phase = Phase::Faulted;
            return 0;
        }
        glFlush();
        if (!CheckNative("ExecuteCommandLists.Flush"))
        {
            m_WorkFailed = true;
            return 0;
        }
        record.submissionId = ++m_LastSubmissionId;
        for (std::size_t index = 0; index < count; ++index)
        {
            auto *packet = record.packets[index];
            packet->phase = PacketPhase::Submitted;
            packet->tracker.CommandListSubmitted();
            nativeLists[index]->m_Packet = nullptr;
        }
        return record.submissionId;
    }

    void OpenGLDevice::RetirePacket(UploadWorkPacket &packet, bool nativeAvailable) noexcept
    {
        packet.tracker.Reset();
        for (auto &chunk : packet.chunks)
        {
            if (nativeAvailable && m_NativeUsable && chunk->name != 0)
            {
                m_ContextState.Invalidate();
                if (chunk->mapping != nullptr)
                {
                    const auto valid = glUnmapNamedBuffer(chunk->name);
                    nativeAvailable = CheckNative("RetireUpload.Unmap", chunk->name);
                    if (valid != GL_TRUE)
                    {
                        if (nativeAvailable)
                        {
                            Report("RetireUpload.Unmap", "NativeFailure: mapped storage contents became invalid");
                        }
                        m_Phase = Phase::Faulted;
                        m_WorkFailed = true;
                        nativeAvailable = false;
                    }
                }
                if (nativeAvailable && m_NativeUsable)
                {
                    glDeleteBuffers(1, &chunk->name);
                    nativeAvailable = CheckNative("RetireUpload.Delete", chunk->name);
                }
            }
            m_UploadCapacity -= chunk->capacity;
        }
        // 先解除 lease 计数再 Release wrapper; 后者只入原生删除 FIFO.
        packet.resources.clear();
        for (auto &slot : m_Packets)
        {
            if (slot.get() == &packet)
            {
                slot.reset();
                return;
            }
        }
        std::terminate();
    }

    bool OpenGLDevice::SealOrphans()
    {
        std::array<UploadWorkPacket *, UploadPacketLimit> issued{};
        std::size_t count = 0;
        for (auto &slot : m_Packets)
        {
            if (!slot || !slot->orphaned.load(std::memory_order_acquire) || slot->phase == PacketPhase::Submitted)
            {
                continue;
            }
            if (!slot->issuedWork)
            {
                RetirePacket(*slot, true);
                if (m_WorkFailed || !m_NativeUsable)
                {
                    return false;
                }
            }
            else
            {
                issued[count++] = slot.get();
            }
        }
        if (count == 0)
        {
            return true;
        }
        if (m_Submissions->count == UploadPacketLimit)
        {
            return false;
        }
        auto &record = m_Submissions->records[(m_Submissions->head + m_Submissions->count) % UploadPacketLimit];
        record = {};
        record.packets = issued;
        record.count = count;
        record.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        ++m_Submissions->count;
        for (std::size_t index = 0; index < count; ++index)
        {
            issued[index]->phase = PacketPhase::Submitted;
        }
        if (!CheckCompletionFence(record.sync, "RetireOrphan.Fence"))
        {
            m_WorkFailed = true;
            m_Phase = Phase::Faulted;
            return false;
        }
        glFlush();
        if (!CheckNative("RetireOrphan.Flush"))
        {
            m_WorkFailed = true;
            return false;
        }
        return true;
    }

    bool OpenGLDevice::WaitSync(GLsync sync, std::chrono::steady_clock::time_point deadline)
    {
        for (;;)
        {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - std::chrono::steady_clock::now())
                    .count();
            if (remaining <= 0)
            {
                return false;
            }
            const auto timeout = std::min<GLuint64>(GLuint64(remaining), 1'000'000);
            const auto status = glClientWaitSync(sync, 0, timeout);
            const bool queried = CheckNative("WaitForCompletion");
            if (!queried || status == GL_WAIT_FAILED)
            {
                m_WorkFailed = true;
                m_Phase = Phase::Faulted;
                if (queried)
                {
                    Report("WaitForCompletion", "NativeFailure: completion wait failed");
                }
                return false;
            }
            if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
            {
                return true;
            }
        }
    }

    bool OpenGLDevice::CollectWork(bool wait)
    {
        if (m_WorkFailed || !SealOrphans())
        {
            return false;
        }
        // 一次显式收拢共享截止点, 不因逐条 fence 完成而重新获得五秒等待预算.
        const auto deadline =
            wait ? std::chrono::steady_clock::now() + std::chrono::seconds(5) : std::chrono::steady_clock::time_point{};
        while (m_Submissions->count != 0)
        {
            auto &record = m_Submissions->records[m_Submissions->head];
            bool complete = false;
            if (wait)
            {
                complete = WaitSync(record.sync, deadline);
            }
            else
            {
                const auto status = glClientWaitSync(record.sync, 0, 0);
                const bool queried = CheckNative("RunGarbageCollection.Poll");
                if (!queried || status == GL_WAIT_FAILED)
                {
                    m_WorkFailed = true;
                    m_Phase = Phase::Faulted;
                    if (queried)
                    {
                        Report("RunGarbageCollection.Poll", "NativeFailure: completion poll failed");
                    }
                    return false;
                }
                complete = status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED;
            }
            if (!complete)
            {
                return false;
            }
            glDeleteSync(record.sync);
            const bool clean = CheckNative("RetireSubmission.Sync");
            // sync 删除失败不撤销已经取得的完成证明, 但禁止后续未知的 GL 清理.
            for (std::size_t index = 0; index < record.count; ++index)
            {
                RetirePacket(*record.packets[index], clean && m_Phase != Phase::Faulted);
            }
            m_CompletedSubmissionId = std::max(m_CompletedSubmissionId, record.submissionId);
            record = {};
            m_Submissions->head = (m_Submissions->head + 1) % UploadPacketLimit;
            --m_Submissions->count;
            if (!clean || m_WorkFailed || m_Phase == Phase::Faulted)
            {
                m_WorkFailed = true;
                return false;
            }
        }
        return true;
    }

    void OpenGLDevice::RunGarbageCollection()
    {
        if (RequireContext("RunGarbageCollection", true) && CheckNative("RunGarbageCollection"))
        {
            if (!m_WorkFailed && m_Phase != Phase::Faulted)
            {
                static_cast<void>(CollectWork(false));
            }
            Drain(true);
        }
    }

    bool OpenGLDevice::WaitForIdle()
    {
        if (!BeginNative("WaitForIdle"))
        {
            return false;
        }
        const auto sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (!CheckCompletionFence(sync, "WaitForIdle.Fence"))
        {
            m_WorkFailed = true;
            m_Phase = Phase::Faulted;
            return false;
        }
        glFlush();
        bool complete = CheckNative("WaitForIdle.Flush");
        if (complete)
        {
            complete = WaitSync(sync, std::chrono::steady_clock::now() + std::chrono::seconds(5));
        }
        bool clean = false;
        if (m_NativeUsable)
        {
            glDeleteSync(sync);
            clean = CheckNative("WaitForIdle.DeleteSync");
        }
        if (!clean)
        {
            m_WorkFailed = true;
        }
        // 只退休已有提交/交接包, 活 recording 即使已物理完成也继续持有自己的存储.
        return complete && clean && CollectWork(false);
    }

    ShutdownPrecondition OpenGLDevice::CheckShutdown(unsigned long knownRoots) const noexcept
    {
        if (std::this_thread::get_id() != m_OwnerThread || knownRoots == 0)
        {
            return ShutdownPrecondition::InvalidState;
        }
        if (const_cast<OpenGLDevice *>(this)->GetRefCount() != knownRoots + m_InternallyRetainedWrappers)
        {
            return ShutdownPrecondition::ResourceInUse;
        }
        for (const auto &slot : m_Packets)
        {
            if (slot)
            {
                for (const auto &lease : slot->resources)
                {
                    if (!lease.OnlyInternalOwners())
                    {
                        return ShutdownPrecondition::ResourceInUse;
                    }
                }
            }
        }
        return ShutdownPrecondition::Ready;
    }

    DrainResult OpenGLDevice::DrainWork(ShutdownMode mode, std::uint32_t *nativeError)
    {
        if (nativeError != nullptr)
        {
            *nativeError = m_FirstNativeError;
        }
        if (std::this_thread::get_id() != m_OwnerThread || mode > ShutdownMode::ContextUnavailable ||
            m_ActiveRecording.load() != 0)
        {
            return DrainResult::InvalidState;
        }
        if (m_Phase == Phase::Stopped)
        {
            return DrainResult::Ready;
        }
        for (const auto &slot : m_Packets)
        {
            if (slot && slot->phase != PacketPhase::Submitted && !slot->orphaned.load(std::memory_order_acquire))
            {
                return DrainResult::InvalidState;
            }
        }
        bool nativeAvailable = mode == ShutdownMode::Normal && m_NativeUsable;
        if (nativeAvailable && !RequireContext("DrainDeviceWork", true))
        {
            return DrainResult::InvalidState;
        }
        const bool faulted = m_Phase == Phase::Faulted || m_WorkFailed;
        m_Phase = Phase::Draining;
        if (nativeAvailable)
        {
            const bool healthy = CheckNative("DrainDeviceWork");
            nativeAvailable = m_NativeUsable;
            if (healthy && !faulted && !m_WorkFailed)
            {
                if (!CollectWork(true) && !m_WorkFailed && m_Phase != Phase::Faulted)
                {
                    return DrainResult::Pending;
                }
            }
            // 完成轮询/退休自身也可能确认 loss; 后续退出调用不能沿用进入 collector 前的健康快照.
            nativeAvailable = nativeAvailable && m_NativeUsable;
            if (nativeAvailable && (faulted || !healthy || m_WorkFailed || m_Phase == Phase::Faulted))
            {
                // 只在关闭且无可靠 fence 时使用一次全流收拢. 驱动调用本身无硬实时保证.
                glFinish();
                nativeAvailable = CheckNative("DrainDeviceWork.ExitFinish") && m_NativeUsable;
                // 再次失败后由 owner 销毁 context. 不重试 GL, 不推进 completed ID 或 tracker metadata.
            }
        }
        if (m_Submissions)
        {
            for (auto &record : m_Submissions->records)
            {
                if (nativeAvailable && record.sync != nullptr)
                {
                    glDeleteSync(record.sync);
                    nativeAvailable = CheckNative("DrainDeviceWork.DeleteSync");
                }
                record = {};
            }
            m_Submissions->count = 0;
        }
        for (auto &slot : m_Packets)
        {
            if (slot)
            {
                RetirePacket(*slot, nativeAvailable);
                nativeAvailable = nativeAvailable && m_NativeUsable && m_Phase != Phase::Faulted;
            }
        }
        Drain(nativeAvailable);
        m_Phase = Phase::Stopped;
        if (nativeError != nullptr)
        {
            *nativeError = m_FirstNativeError;
        }
        return DrainResult::Ready;
    }

    bool OpenGLDevice::Shutdown(ShutdownMode mode, std::uint32_t *nativeError)
    {
        if (nativeError != nullptr)
        {
            *nativeError = 0;
        }
        if (CheckShutdown(1) != ShutdownPrecondition::Ready)
        {
            Report("ShutdownDevice", "ResourceInUse or InvalidState: release external owners first");
            return false;
        }
        if (m_Phase == Phase::Stopped)
        {
            return true;
        }
        return DrainWork(mode, nativeError) == DrainResult::Ready && m_FirstNativeError == GL_NO_ERROR && !m_WorkFailed;
    }

    ShutdownPrecondition CheckShutdownPreconditions(RHI::Device *device, unsigned long knownRoots) noexcept
    {
        auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native != nullptr ? native->CheckShutdown(knownRoots) : ShutdownPrecondition::InvalidState;
    }

    DrainResult DrainDeviceWork(RHI::Device *device, ShutdownMode mode, std::uint32_t *nativeError)
    {
        if (nativeError != nullptr)
        {
            *nativeError = 0;
        }
        auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native != nullptr ? native->DrainWork(mode, nativeError) : DrainResult::InvalidState;
    }
}
