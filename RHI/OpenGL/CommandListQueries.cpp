#include "CommandList.hpp"

/// @file
/// @brief 在原即时 recording 发出 timestamp 对, 包 lease 先于点取得, 不保存重放命令.

#include <new>

#include "Queries.hpp"

namespace Physara::RHI::OpenGL
{
    // 依次确认 recording/归属/阶段, 准备包 lease 与查询索引, 成功后才改变查询状态并发起时间戳.
    void OpenGLCommandList::BeginTimerQuery(TimerQuery *query)
    {
        auto *packet = Recording("BeginTimerQuery");
        if (packet == nullptr)
        {
            return;
        }
        auto *native = dynamic_cast<OpenGLTimerQuery *>(query);
        if (native == nullptr || native->Owner() != m_Device.Get())
        {
            m_Device->Report("BeginTimerQuery", "InvalidArgument: null or foreign query implementation/Device");
            return;
        }
        native->ObserveRecording();
        if ((native->m_Phase != OpenGLTimerQuery::Phase::Idle && native->m_Phase != OpenGLTimerQuery::Phase::Ready) ||
            native->m_LastRecordingId == packet->recordingId)
        {
            m_Device->Report("BeginTimerQuery", "InvalidState: unresolved interval or repeated use in this recording");
            return;
        }
        // resources 是强引用权威, timers 只提供 Close/退休时的遍历入口; 后者准备失败不发出新点.
        if (RetainResource(native) == nullptr)
        {
            return;
        }
        try
        {
            packet->timers.push_back(native);
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report("BeginTimerQuery", "CPU allocation failed before timestamp");
            return;
        }
        if (!m_Device->BeginNative("BeginTimerQuery"))
        {
            packet->invalid = true;
            return;
        }
        native->m_Packet = packet;
        // Ready 结果已解析, 下一次合法 recording 的 Begin 明确消费前值, 旧包仍保留自己的 lease.
        native->m_LastRecordingId = packet->recordingId;
        native->m_Phase = OpenGLTimerQuery::Phase::Begun;
        native->m_Seconds = 0;
        // 标记可能发出, 失败也不能撤销驱动已消费的点或提前释放其名字.
        packet->issuedWork = true;
        glQueryCounter(native->m_Deletion[0]->name, GL_TIMESTAMP);
        if (!m_Device->CheckNative("BeginTimerQuery.Timestamp", native->m_Deletion[0]->name))
        {
            native->m_Phase = OpenGLTimerQuery::Phase::Failed;
            packet->invalid = true;
        }
    }

    // End 必须匹配当前包中的 Begin, 复用其已取得的 lease; 原生成功后才形成可解析的完整区间.
    void OpenGLCommandList::EndTimerQuery(TimerQuery *query)
    {
        auto *packet = Recording("EndTimerQuery");
        if (packet == nullptr)
        {
            return;
        }
        auto *native = dynamic_cast<OpenGLTimerQuery *>(query);
        if (native == nullptr || native->Owner() != m_Device.Get())
        {
            m_Device->Report("EndTimerQuery", "InvalidArgument: null or foreign query implementation/Device");
            return;
        }
        if (native->m_Phase != OpenGLTimerQuery::Phase::Begun || native->m_Packet != packet ||
            native->m_LastRecordingId != packet->recordingId)
        {
            m_Device->Report("EndTimerQuery", "InvalidState: matching Begin in this recording required");
            return;
        }
        if (!m_Device->BeginNative("EndTimerQuery"))
        {
            packet->invalid = true;
            return;
        }
        packet->issuedWork = true;
        glQueryCounter(native->m_Deletion[1]->name, GL_TIMESTAMP);
        if (!m_Device->CheckNative("EndTimerQuery.Timestamp", native->m_Deletion[1]->name))
        {
            native->m_Phase = OpenGLTimerQuery::Phase::Failed;
            packet->invalid = true;
            return;
        }
        native->m_Phase = OpenGLTimerQuery::Phase::Ended;
    }
} // namespace Physara::RHI::OpenGL
