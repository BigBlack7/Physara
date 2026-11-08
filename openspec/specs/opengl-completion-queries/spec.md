# opengl-completion-queries Specification

## Purpose

定义公共 RHI 在单即时 Graphics 设备上的 Event/Timer 查询行为. 调用者能够区分已接受提交的完成快照、GPU 计时间隔的可用结果及真实失败, 并在明确的设备归属、录制身份和退休条件下有界复用查询对象, 不把查询就绪当成未提交工作或整批资源已经完成.

## Requirements

### Requirement: Capture accepted graphics submission snapshots

EventQuery SHALL 捕获设置时该设备 Graphics 队列最后已接受的提交身份. 尚未接受的即时前缀和设置后的提交 SHALL 不改变其观察目标. 已捕获非零身份的查询 SHALL 在重新设置前显式 Reset; 被拒绝的设置 SHALL 保留原快照.

新建或 Reset 后的零身份 SHALL 表示没有捕获待等待提交, 在合法调用前提下 Poll 为 true、Wait 无工作返回. 该结果 SHALL 不证明任何未提交命令已完成. Reset SHALL 只清除观察者, 不取消或完成所观察的 GPU 工作.

#### Scenario: Keep a snapshot while later work is issued
- **WHEN** 提交 A 被接受后设置 Event, 随后录制或接受另一次提交 B
- **THEN** Event 继续只观察 A, 不被 B 或当前未提交命令改成队列尾的完成条件

#### Scenario: Observe an empty submission history
- **WHEN** 健康设备上的 Event 尚未设置、已经 Reset, 或 Set 时尚无已接受提交
- **THEN** Poll 为 true 且 Wait 不插入或等待虚构工作, 即时录制前缀的引用/提交状态保持原样

#### Scenario: Reset a pending observer without cancelling its work
- **WHEN** 调用者 Reset 一个仍在等待的 Event, 然后在合法边界重新设置
- **THEN** 旧观察身份被清除且新快照符合本次设置时刻, 旧提交的完成值和保活不因 Reset 改变; 未 Reset 的非零快照不被隐式覆盖

### Requirement: Resolve event snapshots through the existing completion authority

Event Poll SHALL 非阻塞地使用同一提交完成事实. Pending SHALL 返回 false 且不释放未证明完成的引用; true SHALL 只对应合法零快照或捕获身份已经真实完成. 显式 Wait SHALL 等待该快照而非更晚提交, 成功需要真实证明, 应用层等待 SHALL 有单次有限期限.

失败 SHALL 通过既有诊断区别于普通 Pending, 不发布假完成或无限等待. 超时 SHALL 保留可继续观察的快照和未完成工作, 不推断设备丢失. 查询 SHALL 不建立第二份完成序列或改变已有提交身份.

#### Scenario: Poll an incomplete captured submission
- **WHEN** 捕获身份的实际完成检查返回 Pending
- **THEN** Poll 立即返回 false, 不等待、推进完成或释放该提交仍需要的对象与上传区段

#### Scenario: Wait for the captured prefix only
- **WHEN** A 的 Event 被显式等待, 且 A 之后还有未证明完成的提交 B
- **THEN** 等待取得 A 完成后即可返回, 不再等待 B; 对 A 的重复观察不重复退休其资源

#### Scenario: Distinguish timeout and failed completion
- **WHEN** 显式等待到期或原生完成查询实际失败
- **THEN** 前者有超时诊断并保留 Pending 工作, 后者按既有停止/退出合同保留未证明完成的工作并报告失败, 两者均不成为成功完成

### Requirement: Record valid timer intervals in one immediate recording

TimerQuery SHALL 用同一合法 recording 中的 Begin/End 定义真实 GPU 计时间隔, 时间点在录制调用时发出, Execute SHALL 不重放. 不同查询对象 SHALL 可以表示重叠区间. 同一查询 SHALL 不在同一次 recording 重复使用, 也不在前次未解析时被另一次 recording 覆盖.

没有匹配 Begin 的 End、重复或跨 recording 配对 SHALL 在受影响时间点之前拒绝, 不覆盖先前有效状态. Close 时尚未配对的 Begin 或 recording 被放弃/失败 SHALL 不产生正常有效计时结果; 已发时间点和其它前缀仍受现有退休合同保护.

#### Scenario: Time real draws with overlapping independent queries
- **WHEN** 两个不同 Timer 在合法 recording 中交错 Begin/End 并覆盖真实 Draw, 随后 Close/Execute
- **THEN** 各自保留其实际 GPU 边界, 每个点只发出一次, 不因共用设备而混淆区间

#### Scenario: Reject an illegal timer transition
- **WHEN** 调用者无匹配 Begin 就 End、重复 Begin/End, 或试图跨 recording/在同一 recording 再次使用同一查询
- **THEN** 新的非法时间点不发出, 已有查询结果/使用和已发前缀不被替换或提前释放

#### Scenario: Close or abandon an incomplete interval
- **WHEN** 已发 Begin 后未 End 就 Close, 或该 recording 后来被放弃/置为无效
- **THEN** 不发布该次正常计时结果或成功提交, 不自动补结束点, 已发使用保留到真实退休或合法退出

### Requirement: Return available GPU durations in public seconds

Timer Poll SHALL 只在本次完整区间的两个 GPU 结果均可用时返回 true 并提供计时结果. 尚不可用时 SHALL 非阻塞返回 false, 不读取可能等待的结果值, 不缓存伪零耗时. 公开结果 SHALL 是按实际 counter 位宽/有效范围解释的 GPU 时间差, 以秒表示; 合法零时间与失败占位值 SHALL 能由就绪状态和诊断区分.

正常消费 SHALL 使用先 Poll 后 Get. 显式 Get 尚未就绪的完整区间可以等待, 但 SHALL 有有限期限并处理实际失败, 不以全设备等待代替查询. 失败或超时 SHALL 不缓存 Ready、不推进提交完成身份或释放未证明安全的存储. 已解析结果 SHALL 在明确复位/下一次合法使用前保持可读.

#### Scenario: Leave unavailable timing pending
- **WHEN** 一个完整区间的任一结果尚不可用
- **THEN** Poll 返回 false 且不等待、不读其结果值, 调用者不能把它当作 0 秒测量

#### Scenario: Read an available measured interval
- **WHEN** 两个实际 GPU 时间点均可用且区间处于设备声明的计时范围内
- **THEN** Get 返回有限非负的秒结果并保持稳定, 来源是完整 GPU 数值及其单位, 不使用 CPU 时钟、固定值或截断的时间戳代替

#### Scenario: Bound an explicit timing read
- **WHEN** 调用者对完整 Pending 区间显式 Get, 随后取得结果、到期或遇到原生错误
- **THEN** 只在真实 available 后读取并缓存秒值; 到期/错误有明确诊断并保留必要使用, 不无限轮询或虚构成功

### Requirement: Bound and retire query ownership before destructive reuse

查询 SHALL 保持其 Device 有效. Timer 使用 SHALL 在原生发出前取得现有 recording/提交协议所需引用, caller Release 或计时 Ready SHALL 不替代整批完成. 未解析的 Timer Reset/覆盖 SHALL 被拒绝; 成功复位后的新使用 SHALL 不读取旧结果, 同一次 recording 的重复使用限制仍生效. 失败/放弃区间 SHALL 只在原使用已经安全退休后恢复可用.

原生计时存储及内部保留 SHALL 有明确有限预算, 预算耗尽 SHALL 在新使用之前失败且不等待或破坏旧对象. 最终 Release SHALL 不在 foreign thread 执行 GL 或申请退休存储. 只有内部引用 SHALL 不阻止受控关闭, 真实外部 query owner SHALL 仍阻止破坏性关闭; 失效 context SHALL 不再被查询或原生删除.

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

### Requirement: Validate query inputs and preserve primary failures

所有公开查询入口 SHALL 在使用前核对本次有效对象、实现/Device、owner thread/current context、队列或 recording 前提, Debug 和 Release 均成立. 无效输入 SHALL 在受影响原生工作前明确拒绝. 新对象 SHALL 仅在 CPU/native 准备全部成功后发布, 不可用计时能力 SHALL 不返回假对象或结果.

原生发出或结果获取失败 SHALL 保留操作和首错, 依现有设备停止/退出合同处理已发前缀; CPU 准备失败 SHALL 不执行该次操作. 清理诊断 SHALL 不掩盖原始失败, 不增加回调异常传播或对致命 noexcept OOM 的恢复保证.

#### Scenario: Reject a foreign or illegal public query call
- **WHEN** raw 或 Validation 路径收到空/foreign 实现、另一设备的 query, 或处于错误 thread/context、队列/recording 状态
- **THEN** 受影响调用明确拒绝且不触碰该对象的原生名字, 不改变其 owner 或已有合法使用

#### Scenario: Fail creation or native timing work
- **WHEN** 查询准备/分配失败, 实际能力不可用, 或时间点/结果获取遇到原生错误
- **THEN** 未发布对象被正确清理, 受影响操作不伪成功; 已发前缀及尚未证明安全的 query 使用继续保留, 原始失败仍可定位
