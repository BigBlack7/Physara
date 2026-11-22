# Spec Delta

## MODIFIED Requirements

### Requirement: Return available GPU durations in public seconds

Timer Poll SHALL 只在本次包含有效 Begin/End 的录制已被接受且两个 GPU 结果均可用时返回 true 并提供计时结果. 仅有实发时间点或只为内部退休设置的 fence SHALL 不发布计时结果. 尚不可用时 SHALL 非阻塞返回 false, 不读取可能等待的结果值, 不缓存伪零耗时. 公开结果 SHALL 是按实际 counter 位宽/有效范围解释的 GPU 时间差, 以秒表示; 合法零时间与失败占位值 SHALL 能由就绪状态和诊断区分.

正常消费 SHALL 使用先 Poll 后 Get. 显式 Get 已接受但尚未就绪的完整区间可以等待, 但 SHALL 有有限期限并处理实际失败, 不以全设备等待代替查询. 失败或超时 SHALL 不缓存 Ready、不推进提交完成身份或释放未证明安全的存储. 已解析结果 SHALL 在明确复位/下一次合法使用前保持可读.

#### Scenario: Leave unavailable timing pending
- **WHEN** 一个完整区间的任一结果尚不可用
- **THEN** Poll 返回 false 且不等待、不读其结果值, 调用者不能把它当作 0 秒测量

#### Scenario: Read an available measured interval
- **WHEN** 两个实际 GPU 时间点均可用且区间处于设备声明的计时范围内
- **THEN** Get 返回有限非负的秒结果并保持稳定, 来源是完整 GPU 数值及其单位, 不使用 CPU 时钟、固定值或截断的时间戳代替

#### Scenario: Bound an explicit timing read
- **WHEN** 调用者对完整 Pending 区间显式 Get, 随后取得结果、到期或遇到原生错误
- **THEN** 只在真实 available 后读取并缓存秒值; 到期/错误有明确诊断并保留必要使用, 不无限轮询或虚构成功

#### Scenario: Leave an issued interval unpublished until its recording is accepted
- **WHEN** 一个有效区间已经 End，所属列表仍打开或已 Close 但未被接受，且驱动可能已经生成 timestamp
- **THEN** Poll 仍非阻塞返回 false，显式 Get 不等待该录制；有效提交后开始按本次两个结果的 available 状态解析，放弃的列表不发布秒数

### Requirement: Bound and retire query ownership before destructive reuse

查询 SHALL 保持其 Device 有效. Timer 使用 SHALL 在原生发出前取得现有 recording/提交协议所需引用, caller Release 或计时 Ready SHALL 不替代整批完成. 未解析的 Timer Reset/覆盖 SHALL 被拒绝; 成功复位后的新使用 SHALL 不读取旧结果, 同一次 recording 的重复使用限制仍生效. 失败/放弃区间 SHALL 只在原使用已经安全退休后恢复可用.

每设备 Event 观察者的 CPU 对象与原生计时存储 SHALL 各有明确有限预算, 预算耗尽 SHALL 在新建对象之前失败且不等待或破坏旧对象. Event 的容量由观察者最后的 CPU owner 归还，Reset 保留对象；Timer 的原生容量归还仍以实际删除或受控退出为准. 最终 Release SHALL 不在 foreign thread 执行 GL 或申请退休存储. 只有内部引用 SHALL 不阻止受控关闭, 真实外部 query owner SHALL 仍阻止破坏性关闭; 失效 context SHALL 不再被查询或原生删除.

#### Scenario: Release query handles while work remains retained
- **WHEN** caller 在发出 Timer 点和 Draw 后释放 Timer/列表句柄
- **THEN** 已发使用由既有 owner 保护到真实退休, worker 最后 Release 只交接, 原生名字与预算不提前删除/复用

#### Scenario: Reuse a resolved query without stale results
- **WHEN** 前次区间已解析并明确复位/消费后在另一 recording 复用, 或放弃区间已退休后复位
- **THEN** 新区间只提供自己的结果, 旧包收拢不覆盖新使用; Pending 使用的 Reset/覆盖仍被拒绝

#### Scenario: Exhaust and later recover timing storage
- **WHEN** Timer 存储达到声明预算, 然后已有使用完成且其原生对象真正退休
- **THEN** 超预算请求先明确失败且不影响旧使用, 只有安全退休后预算才可用于后续对象

#### Scenario: Separate external ownership and context-unavailable exit
- **WHEN** 有外部 Event/Timer owner 请求关闭, 或已停止用户后进入确认不可用 context 的退出路径
- **THEN** 前者可重试拒绝且设备保持有效, 后者只清 CPU 状态并交既有环境 owner 退出, 不再 poll/delete GL 或宣称失败工作已完成

#### Scenario: Return an event observer slot independently of its captured work
- **WHEN** Event 达到设备声明的对象容量，且释放其中一个仍在观察待完成提交的 Event
- **THEN** 新建超限时明确失败，最后 Release 后可为新 Event 分配容量，旧提交的完成身份和在途工作保持原有状态
