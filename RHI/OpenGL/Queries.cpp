#include "Queries.hpp"

/// @file
/// @brief 创建有界查询对象, 通过唯一提交完成事实观察 Event, 仅在 available 后解析 GPU 时间戳.

#include <chrono>
#include <memory>
#include <new>
#include <thread>
#include <utility>

#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    // 公共查询入口共用的归属边界, 拒绝后不读取 foreign 对象的私有状态.
    template <typename Native, typename Public>
    static Native *OwnedQuery(OpenGLDevice &device, Public *query, const char *operation)
    {
        if (!device.RequireContext(operation))
        {
            return nullptr;
        }
        auto *native = dynamic_cast<Native *>(query);
        if (native == nullptr || native->Owner() != &device)
        {
            device.Report(operation, "InvalidArgument: null or foreign query implementation/Device");
            return nullptr;
        }
        return native;
    }

    // 两个节点在取得任何原生名字前准备, 构造失败只需释放已构造的 CPU 成员.
    OpenGLTimerQuery::OpenGLTimerQuery(OpenGLDevice *device, GLint counterBits)
        : m_Device(device), m_Deletion{std::make_unique<NativeDeletion>(NativeKind::Query),
                                       std::make_unique<NativeDeletion>(NativeKind::Query)},
          m_CounterBits(counterBits)
    {
    }

    // 包 lease 已撤销才可能最终析构; 发布过的名字入队, 预算到 owner 实际删除时才归还.
    OpenGLTimerQuery::~OpenGLTimerQuery()
    {
        for (auto &record : m_Deletion)
        {
            m_Device->ReleaseAllocation(std::move(record), m_Published);
        }
    }

    // owner 观察录制失败或 acquire 接收 worker 的放弃事实, 不提前解除原包使用.
    void OpenGLTimerQuery::ObserveRecording() noexcept
    {
        if (m_Packet != nullptr &&
            (m_Packet->orphaned.load(std::memory_order_acquire) || m_Packet->invalid || m_Packet->tracker.HasErrors()))
        {
            m_Phase = Phase::Failed;
            m_Seconds = 0;
        }
    }

    // Close/orphan 收取只同步失败, 真正退休时才断开借用, 且须早于 tracker/lease/包的销毁.
    // 已解析 query 可以先在后继包复用, 旧包的迟到处理必须同时匹配包指针和 recording 身份.
    void OpenGLDevice::ReconcileQueries(UploadWorkPacket &packet, bool retired) noexcept
    {
        for (auto *query : packet.timers)
        {
            if (query->m_Packet == &packet && query->m_LastRecordingId == packet.recordingId)
            {
                query->ObserveRecording();
                if (retired)
                {
                    query->m_Packet = nullptr;
                }
            }
        }
    }

    // Event 仅分配强持 Device 的 CPU 观察者, 没有独立 fence 或工作保活队列.
    EventQueryHandle OpenGLDevice::CreateEventQuery()
    {
        if (!RequireContext("CreateEventQuery"))
        {
            return nullptr;
        }
        try
        {
            return EventQueryHandle::Create(new OpenGLEventQuery(this));
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateEventQuery", "CPU allocation failed");
            return nullptr;
        }
    }

    // 只截取已接受提交身份; 非零旧快照须显式 Reset, 被拒绝的设置不改原快照.
    void OpenGLDevice::SetEventQuery(EventQuery *query, CommandQueue queue)
    {
        auto *native = OwnedQuery<OpenGLEventQuery>(*this, query, "SetEventQuery");
        if (native == nullptr)
        {
            return;
        }
        if (queue != CommandQueue::Graphics || native->m_SubmissionId != 0)
        {
            Report("SetEventQuery",
                   "InvalidState: Graphics queue and Reset before replacing a nonzero snapshot required");
            return;
        }
        native->m_SubmissionId = m_LastSubmissionId;
    }

    // 通过同一 FIFO 非阻塞证明捕获前缀完成, 后续录制/提交不扩大观察目标.
    bool OpenGLDevice::PollEventQuery(EventQuery *query)
    {
        auto *native = OwnedQuery<OpenGLEventQuery>(*this, query, "PollEventQuery");
        if (native == nullptr)
        {
            return false;
        }
        // 零快照不含任何已接受工作, 不能用它解除未提交前缀的引用或上传存储.
        if (native->m_SubmissionId == 0)
        {
            return true;
        }
        return BeginNative("PollEventQuery") && CollectWork(false, native->m_SubmissionId);
    }

    // 显式等待仅到捕获 ID, collector 内整次调用共用五秒期限; 超时仍保留快照和包.
    void OpenGLDevice::WaitEventQuery(EventQuery *query)
    {
        auto *native = OwnedQuery<OpenGLEventQuery>(*this, query, "WaitEventQuery");
        if (native == nullptr || native->m_SubmissionId == 0 || !BeginNative("WaitEventQuery"))
        {
            return;
        }
        if (!CollectWork(true, native->m_SubmissionId) && !m_WorkFailed && m_Phase == Phase::Active)
        {
            Report("WaitEventQuery", "Timeout: captured submission remains pending after five seconds");
        }
    }

    // 只清 CPU 观察身份, 允许放弃 Pending 观察, 不影响 GPU 工作和其它观察者.
    void OpenGLDevice::ResetEventQuery(EventQuery *query)
    {
        if (auto *native = OwnedQuery<OpenGLEventQuery>(*this, query, "ResetEventQuery"))
        {
            // 允许放弃 Pending 观察, 不取消提交, 不改 Device 完成身份或包保活.
            native->m_SubmissionId = 0;
        }
    }

    // 先查入口/名字预算/实际位宽, 再准备 CPU 节点并取得整对名字, 全部成功才发布.
    TimerQueryHandle OpenGLDevice::CreateTimerQuery()
    {
        if (!RequireContext("CreateTimerQuery"))
        {
            return nullptr;
        }
        if (!glCreateQueries || !glDeleteQueries || !glQueryCounter || !glGetQueryiv || !glGetQueryObjectiv ||
            !glGetQueryObjectui64v || !glBindBuffer)
        {
            Report("CreateTimerQuery", "Unsupported: timestamp query entry missing");
            return nullptr;
        }
        if (m_TimerNames > TimerNameLimit - 2)
        {
            ReportBudget("CreateTimerQuery", 2, m_TimerNames, TimerNameLimit);
            return nullptr;
        }
        if (!BeginNative("CreateTimerQuery"))
        {
            return nullptr;
        }
        GLint bits = 0;
        glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
        if (!CheckNative("CreateTimerQuery.CounterBits"))
        {
            return nullptr;
        }
        if (bits <= 0 || bits > 64)
        {
            Report("CreateTimerQuery", "Unsupported: timestamp counter has no supported effective width");
            return nullptr;
        }
        try
        {
            // wrapper 与两个删除节点先准备, 名字一旦取得就有无需分配的回滚/交接位置.
            auto timer = std::make_unique<OpenGLTimerQuery>(this, bits);
            std::array<GLuint, 2> names{};
            glCreateQueries(GL_TIMESTAMP, 2, names.data());
            // 先登记所有已取得名字, 即使原生返回错误/部分结果, 析构也有对应回滚节点和预算事实.
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                timer->m_Deletion[index]->name = names[index];
                m_TimerNames += names[index] != 0 ? 1 : 0;
            }
            if (!CheckNative("CreateTimerQuery.Names"))
            {
                return nullptr;
            }
            if (names[0] == 0 || names[1] == 0)
            {
                m_Phase = Phase::Faulted;
                m_WorkFailed = true;
                Report("CreateTimerQuery", "NativeFailure: incomplete timestamp pair");
                return nullptr;
            }
            timer->m_Published = true;
            return TimerQueryHandle::Create(timer.release());
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateTimerQuery", "CPU allocation failed before native query creation");
            return nullptr;
        }
    }

    // 先观察录制有效性, 再确认两个点均 available, 最后读取完整数值并缓存秒值.
    // false 可为普通 Pending 或已诊断的失败; 本函数不推进提交完成身份或释放包引用.
    bool OpenGLDevice::ResolveTimer(OpenGLTimerQuery &query)
    {
        query.ObserveRecording();
        if (query.m_Phase == OpenGLTimerQuery::Phase::Ready)
        {
            return true;
        }
        if (query.m_Phase != OpenGLTimerQuery::Phase::Ended)
        {
            Report("TimerQuery.Result", "InvalidState: no valid complete timing interval");
            return false;
        }
        // GL_QUERY_BUFFER 非零会把输出参数解释成 buffer offset, 必须先通过唯一缓存建立客户端指针语义.
        m_ContextState.ApplyQueryRead();
        for (const auto &point : query.m_Deletion)
        {
            GLint available = GL_FALSE;
            glGetQueryObjectiv(point->name, GL_QUERY_RESULT_AVAILABLE, &available);
            if (!CheckNative("TimerQuery.Available", point->name))
            {
                query.m_Phase = OpenGLTimerQuery::Phase::Failed;
                return false;
            }
            if (available != GL_TRUE)
            {
                return false;
            }
        }
        std::array<GLuint64, 2> values{};
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            glGetQueryObjectui64v(query.m_Deletion[index]->name, GL_QUERY_RESULT, &values[index]);
            if (!CheckNative("TimerQuery.Result", query.m_Deletion[index]->name))
            {
                query.m_Phase = OpenGLTimerQuery::Phase::Failed;
                return false;
            }
        }
        // unsigned 模差涵盖 counter 回绕, 但调用者的计时区间须小于实际 counter 的一个周期.
        auto nanoseconds = values[1] - values[0];
        if (query.m_CounterBits < 64)
        {
            nanoseconds &= (GLuint64(1) << query.m_CounterBits) - 1;
        }
        query.m_Seconds = float(double(nanoseconds) / 1'000'000'000.0);
        query.m_Phase = OpenGLTimerQuery::Phase::Ready;
        return true;
    }

    // 正常帧的 available-first 入口, 不 flush/等待或为 Pending 读取 QUERY_RESULT.
    bool OpenGLDevice::PollTimerQuery(TimerQuery *query)
    {
        auto *native = OwnedQuery<OpenGLTimerQuery>(*this, query, "PollTimerQuery");
        return native != nullptr && BeginNative("PollTimerQuery") && ResolveTimer(*native);
    }

    // Ready 直接返回稳定缓存; 完整 Pending 区间才进入有期限的显式冷路径等待.
    // 失败占位 0 不写 Ready, 须结合 Poll/诊断与合法零秒测量区分.
    float OpenGLDevice::GetTimerQueryTime(TimerQuery *query)
    {
        auto *native = OwnedQuery<OpenGLTimerQuery>(*this, query, "GetTimerQueryTime");
        if (native == nullptr)
        {
            return 0;
        }
        native->ObserveRecording();
        if (native->m_Phase == OpenGLTimerQuery::Phase::Ready)
        {
            return native->m_Seconds;
        }
        if (native->m_Phase != OpenGLTimerQuery::Phase::Ended)
        {
            Report("GetTimerQueryTime", "InvalidState: no valid complete timing interval");
            return 0;
        }
        if (!BeginNative("GetTimerQueryTime"))
        {
            return 0;
        }
        // 显式冷路径推进已发点, 不插入提交身份或全设备等待; 一次期限不随轮询重置.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        glFlush();
        if (!CheckNative("GetTimerQueryTime.Flush"))
        {
            return 0;
        }
        do
        {
            if (ResolveTimer(*native))
            {
                return native->m_Seconds;
            }
            if (native->m_Phase != OpenGLTimerQuery::Phase::Ended || m_Phase != Phase::Active)
            {
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        Report("GetTimerQueryTime", "Timeout: timing interval remains pending after five seconds");
        return 0;
    }

    // 仅清可消费的结果状态, 保留最近 recording 身份及包关联, 防止同 recording 重用或失败前缀逃逸.
    void OpenGLDevice::ResetTimerQuery(TimerQuery *query)
    {
        auto *native = OwnedQuery<OpenGLTimerQuery>(*this, query, "ResetTimerQuery");
        if (native == nullptr)
        {
            return;
        }
        native->ObserveRecording();
        if (native->m_Phase == OpenGLTimerQuery::Phase::Begun || native->m_Phase == OpenGLTimerQuery::Phase::Ended ||
            (native->m_Phase == OpenGLTimerQuery::Phase::Failed && native->m_Packet != nullptr))
        {
            Report("ResetTimerQuery", "InvalidState: unresolved timing use still retained");
            return;
        }
        native->m_Phase = OpenGLTimerQuery::Phase::Idle;
        native->m_Seconds = 0;
    }
} // namespace Physara::RHI::OpenGL
