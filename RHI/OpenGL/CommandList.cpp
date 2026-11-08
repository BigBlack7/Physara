#include "CommandList.hpp"

/// @file
/// @brief 管理列表 Open/Close 与唯一 recording claim, 未提交包的最终 Release 只交接给 Device, 不发 GL.

#include <limits>
#include <memory>
#include <new>

#include "Queries.hpp"

namespace Physara::RHI::OpenGL
{
    OpenGLCommandList::OpenGLCommandList(OpenGLDevice *device, const CommandListParameters &params) noexcept
        : m_Device(device), m_Desc(params)
    {
    }

    OpenGLCommandList::~OpenGLCommandList()
    {
        if (m_Packet != nullptr)
        {
            m_Device->AbandonPacket(*m_Packet);
        }
    }

    Device *OpenGLCommandList::GetDevice()
    {
        return m_Device.Get();
    }

    const CommandListParameters &OpenGLCommandList::GetDesc()
    {
        return m_Desc;
    }

    // 未交付命令共用的诊断出口, 不修改 recording 或把失败入口替换成 GPU 空操作.
    void OpenGLCommandList::Unsupported(const char *operation)
    {
        if (m_Device->RequireContext(operation))
        {
            m_Device->Report(operation, "Unsupported: outside immediate graphics/transfer profile");
        }
    }

    // 创建逻辑句柄前核对即时 Graphics profile 与 chunk 策略, 实际物理包留到 Open 才准备.
    CommandListHandle OpenGLDevice::CreateCommandList(const CommandListParameters &params)
    {
        if (!RequireContext("CreateCommandList"))
        {
            return nullptr;
        }
        std::size_t aligned = 0;
        if (!params.enableImmediateExecution || params.queueType != CommandQueue::Graphics ||
            params.lifetimeTracker != nullptr)
        {
            Report("CreateCommandList", "Unsupported: immediate Graphics without external lifetime tracker required");
            return nullptr;
        }
        if (params.uploadChunkSize == 0 ||
            !CheckedAlign(params.uploadChunkSize, std::size_t(m_Caps.minMapAlignment), aligned) ||
            aligned > UploadCapacityLimit)
        {
            Report("CreateCommandList", "InvalidArgument: aligned upload chunk exceeds device policy");
            return nullptr;
        }
        try
        {
            return CommandListHandle::Create(new OpenGLCommandList(this, params));
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateCommandList", "CPU allocation failed");
            return nullptr;
        }
    }

    UploadWorkPacket *OpenGLDevice::OpenPacket(std::size_t minimumChunk)
    {
        // 物理包和 tracker 先构造, 成功取得唯一 claim 才发布身份和 slot; 失败不覆盖另一份录制.
        if (m_ActiveRecording.load() != 0)
        {
            Report("Open", "InvalidState: another immediate recording is active");
            return nullptr;
        }
        if (m_LastRecordingId == std::numeric_limits<std::uint64_t>::max())
        {
            Report("Open", "InvalidState: recording identity exhausted");
            return nullptr;
        }
        for (auto &slot : m_Packets)
        {
            if (slot)
            {
                continue;
            }
            try
            {
                auto packet =
                    std::make_unique<UploadWorkPacket>(m_Desc.messageCallback, m_LastRecordingId + 1, minimumChunk);
                std::uint64_t expected = 0;
                if (!m_ActiveRecording.compare_exchange_strong(expected, packet->recordingId))
                {
                    Report("Open", "InvalidState: immediate recording claim changed");
                    return nullptr;
                }
                ++m_LastRecordingId;
                slot = std::move(packet);
                return slot.get();
            }
            catch (const std::bad_alloc &)
            {
                Report("Open", "CPU allocation failed before recording claim");
                return nullptr;
            }
        }
        ReportBudget("Open", 1, UploadPacketLimit, UploadPacketLimit);
        return nullptr;
    }

    void OpenGLDevice::ReleaseClaim(std::uint64_t recordingId) noexcept
    {
        // 仅释放本包的身份, 迟到的旧列表不能解除后来列表的 recording claim.
        static_cast<void>(m_ActiveRecording.compare_exchange_strong(recordingId, 0));
    }

    void OpenGLDevice::AbandonPacket(UploadWorkPacket &packet) noexcept
    {
        // 先释放唯一身份的 claim, 最后发布交接. 此后 worker 不再触碰 packet/lease.
        ReleaseClaim(packet.recordingId);
        packet.orphaned.store(true, std::memory_order_release);
    }

    void OpenGLCommandList::Open()
    {
        // 物理包在先前提交接受后脱离列表; Closed 尚未提交时不可覆盖它.
        if (!m_Device->RequireContext("Open"))
        {
            return;
        }
        if (m_Packet != nullptr)
        {
            m_Device->Report("Open", "InvalidState: recording or unsubmitted Closed packet already owned");
            return;
        }
        m_Packet = m_Device->OpenPacket(m_Desc.uploadChunkSize);
    }

    UploadWorkPacket *OpenGLCommandList::Recording(const char *operation)
    {
        // 所有实际命令共用此入口, CPU 预检也不能绕开 owner/context 和 sticky tracker 错误.
        if (!m_Device->RequireContext(operation))
        {
            return nullptr;
        }
        if (m_Packet == nullptr || m_Packet->phase != PacketPhase::Recording || m_Packet->invalid ||
            m_Packet->tracker.HasErrors() || m_Device->m_ActiveRecording.load() != m_Packet->recordingId)
        {
            m_Device->Report(operation, "InvalidState: no valid owning recording");
            return nullptr;
        }
        return m_Packet;
    }

    void OpenGLCommandList::Close()
    {
        // Faulted 仍允许结束逻辑录制并释放 claim, 但不会让错误包恢复为可正常提交.
        // Close 与实际消费共享依赖入口; 只有接受提交才发布持久 metadata.
        if (!m_Device->RequireContext("Close", true))
        {
            return;
        }
        if (m_Packet == nullptr || m_Packet->phase != PacketPhase::Recording)
        {
            m_Device->Report("Close", "InvalidState: no owning recording");
            return;
        }
        if (!m_Packet->invalid && !m_Packet->tracker.HasErrors())
        {
            // 不自动补齐计时区间. 已复用到其它包的 query 不归本包当前状态检查所有.
            for (auto *query : m_Packet->timers)
            {
                if (query->m_Packet == m_Packet && query->m_Phase == OpenGLTimerQuery::Phase::Begun)
                {
                    m_Packet->invalid = true;
                    m_Device->Report("Close", "InvalidState: unmatched BeginTimerQuery retains issued prefix");
                    break;
                }
            }
        }
        if (!m_Packet->invalid && !m_Packet->tracker.HasErrors())
        {
            m_Packet->tracker.KeepBufferInitialStates();
            m_Packet->tracker.KeepTextureInitialStates();
            static_cast<void>(ConsumeBarriers());
        }
        m_Packet->phase = PacketPhase::Closed;
        // Close 同步结果有效性但不撤销查询/Draw 引用, 后者仍等提交完成或 orphan/退出退休.
        m_Device->ReconcileQueries(*m_Packet, false);
        m_Device->ReleaseClaim(m_Packet->recordingId);
    }

    void OpenGLCommandList::ClearState()
    {
        // 取消逻辑可绘制条件并使 native 事实未知, 资源用途和已发前缀的 lease 继续属于同一包.
        if (Recording("ClearState") != nullptr)
        {
            m_Packet->graphics = {};
            m_Packet->graphicsSet = false;
            m_Packet->viewportSet = false;
            m_Packet->scissorSet = false;
            m_Device->m_ContextState.Invalidate();
        }
    }
}
