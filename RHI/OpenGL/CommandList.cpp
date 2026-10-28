#include "CommandList.hpp"

#include <limits>
#include <memory>
#include <new>

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

    void OpenGLCommandList::Unsupported(const char *operation)
    {
        if (m_Device->RequireContext(operation))
        {
            m_Device->Report(operation, "Unsupported: upload-only command list");
        }
    }

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
            m_Packet->tracker.KeepBufferInitialStates();
            m_Packet->tracker.KeepTextureInitialStates();
            m_Packet->tracker.ClearBarriers();
        }
        m_Packet->phase = PacketPhase::Closed;
        m_Device->ReleaseClaim(m_Packet->recordingId);
    }

    void OpenGLCommandList::ClearState()
    {
        if (Recording("ClearState") != nullptr)
        {
            m_Device->m_ContextState.Invalidate();
        }
    }
}
