# Design

## Context

动机和工作包边界见 [proposal](proposal.md). 2026-11-07 规划起点为 `v2.0` / `6d4dc33107b1b523adbd100846fa1217b7adb9a4`, 创建本 change 前工作区干净. D03-T03 已归档, 以下是实际源码事实, 不是本包已完成的声明:

| 当前事实 | 实际入口 | 本次处理 |
|---|---|---|
| 已有有界物理包、独立 recording/submission 身份、真实 fence/flush 和 FIFO 回收 | [Submission.hpp](../../../../RHI/OpenGL/Submission.hpp)、[Submission.cpp](../../../../RHI/OpenGL/Submission.cpp) | 扩展同一完成权威, Event 等待只等待其捕获的已接受 ID |
| Draw 的 Pipeline/集合/Framebuffer/VB/IB 已纳入包 lease 与复合依赖记账 | [CommandListOwnership.cpp](../../../../RHI/OpenGL/CommandListOwnership.cpp)、[Upload.hpp](../../../../RHI/OpenGL/Upload.hpp) | 加入 Timer 叶资源, 完整验收多包共享、外部 owner 及真实退休 |
| coherent upload chunk 的 used 只增长, 退休前不覆盖; 普通更新已有两种上传路径 | [Upload.cpp](../../../../RHI/OpenGL/Upload.cpp)、[CommandListUpload.cpp](../../../../RHI/OpenGL/CommandListUpload.cpp) | 用两份不同内容的 Draw 观察后续更新, 不借机实现 volatile 版本槽 |
| 公共 Event/Timer Device 方法与两个 Timer 命令仍为 Unsupported | [Unsupported.cpp](../../../../RHI/OpenGL/Unsupported.cpp)、[CommandListUnsupported.cpp](../../../../RHI/OpenGL/CommandListUnsupported.cpp) | 新增实际 Queries 实现, 其余未交付方法继续拒绝 |
| Validation 已检查 query 非空/队列/列表 Open, 随后直接委托 | [ValidationDevice.cpp](../../../../RHI/Validation/ValidationDevice.cpp)、[ValidationCommandList.cpp](../../../../RHI/Validation/ValidationCommandList.cpp) | 保留委托, native 层检查实际实现、Device、阶段和 recording 身份 |
| final Release 使用预分配删除节点, shutdown 区分外部 owner、Pending 和失效 context | [Backend.hpp](../../../../RHI/OpenGL/Backend.hpp)、[Device.cpp](../../../../RHI/OpenGL/Device.cpp)、[RhiDeviceHost.cpp](../../../../Platform/RhiDeviceHost.cpp) | 将查询名字和 wrapper 纳入既有删除/退出, 不新增另一套保活队列 |

合同为 [11.1](../../../../Docs/Architecture.md#retirement)、[11.3](../../../../Docs/Architecture.md#readbacks)、[11.5](../../../../Docs/Architecture.md#shutdown)、[7.4](../../../../Docs/Architecture.md#uploads) 与 [7.6](../../../../Docs/Architecture.md#command-execution). 固定 NVRHI 行为核对 [完成/查询分析](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n10)、本地 `ThirdParty/nvrhi/src/vulkan/vulkan-queries.cpp` 和公共 [Timer 命令合同](../../../../RHI/RHI.hpp): Begin/End 属同一列表, 同一 query 在同一次 recording 不重复使用, 结果单位是秒. 不复制上游无限忙等或仅 Debug assert 的错误边界.

已有 [上传规范](../../../specs/opengl-resource-uploads/spec.md) 的 14 项合同和 [即时绘制规范](../../../specs/opengl-immediate-draw/spec.md) 继续生效. 本包不重新定义同义的 submission/retirement capability; 新规范只定义公共查询, device-resources delta 更新实际开放和交付边界.

## Goals / Non-Goals

**Goals:** Query 以既有完成身份和实际 GPU 结果对外工作; CPU wrapper、原生 query 名字、上传区段和 Draw 依赖各按对应证据退休; 将既有协议在真实 Draw 的正常、失败和退出链中完整验收.

**Non-Goals:** 不引入第二条 timeline、每 query 独立 Event fence、查询管理线程或全局对象 registry. 本包没有 FrameSlot/Renderer 消费者, 轮转由有限数量的公共 query 对象表达, 不提前建渲染统计系统. 不通过公共 staging/map 扩张本包; 像素/数据检查仅在 build 冷路径显式证明完成后读回.

## Decisions

### 1. 一份完成权威, Event 等待有明确截止身份

继续由 OpenGLDevice 拥有 `m_LastSubmissionId`、`m_CompletedSubmissionId`、SubmissionState 和 UploadWorkPacket. Close 只结束 recording, Execute 全批预检后实际 fence/flush 均成功才给出正 ID 并发布 tracker metadata, 不重发 Draw/上传/时间戳.

Event 的 Poll 使用同一零超时收集结果. 显式 Wait 需要在既有 collector/wait 入口增加“完成到捕获 ID 即止”的私有条件, 保持 FIFO 和 Complete/Pending/Failed 映射; 不调用会等待后来命令流尾的 WaitForIdle, 不复制一个查询专用 completedId. Poll 可以顺带收集已完成的前缀, 但不能等后续未完成工作.

Wait 的应用层期限沿用当前单次五秒预算, 循环不重置截止点. 超时报告后保留身份/引用, 不将 timeout 当 lost. 原生调用自身没有硬实时保证. 非法输入不消耗任何提交状态; query 不为没有获得正票据的前缀发布新身份.

备选的每 Event 插 fence 会把快照变成当前命令流 marker, 不能满足既有公共语义, 不采用.

### 2. Event 保存提交快照, reset 只清观察者

Event wrapper 持有 Device 和捕获的 Graphics ID, 不持有第二份 GPU work batch. Set 取最后已接受的 ID, 不受还未提交的即时命令影响, 后续提交不修改快照. 已捕获非零 ID 的 Event 重新设置前先 Reset, 非法重设保留原快照.

新建/Reset 后的零 ID 沿固定源的无待等待工作语义: 在合法设备/调用前提下 Poll 为 true, Wait 为无工作返回. 零 ID 不证明任何未提交 Draw 已完成. Set 在尚无已接受提交时同样捕获零. Reset 可以清除尚在等待的快照, 但不改变 Device 完成值、不取消工作或撤销提交保活. 这种容易误读的例外必须写在接口/实现注释中.

公共 bool/void 签名保持: Pending 为 false 且无失败诊断; 真正失败为 false/void 返回并沿既有回调说明操作和原因, 锁存需要停止的实际故障. 不扩充公共枚举来替代现有错误机制.

### 3. Timer 使用独立时间戳对, 不占用全 context 的 elapsed-query 区间

每 Timer wrapper 拥有两个 timestamp query 名字. Begin/End 在合法 recording 的相应位置执行 `glQueryCounter(..., GL_TIMESTAMP)`, 每个可能发出的点先准备包引用并登记 issuedWork. 不同 Timer 可以覆盖重叠区间; 不使用只能维持一个同目标活动区间的 GL_TIME_ELAPSED 配对.

原生依据为 [OpenGL 4.6 Core 4.2.3/4.3](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=68) 和 [ARB_timer_query](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt). 创建时核对所需 entry points 和实际 timestamp counter bits; 零有效位不提供可用计时. 使用 64 位结果, 按实际位宽解释模差、由纳秒换算为公共秒. 本 profile 的合法计时间隔小于一个实际 counter 周期, 不承诺超出原生范围的长时间测量, 也不以 CPU 时钟填补结果. 实施记录实际位宽和此范围, 不因 64 位返回类型假定硬件必有 64 个有效位.

查询客户端结果前通过唯一 ContextState 建立 `GL_QUERY_BUFFER=0`, 外部互操作的失效仍走现有合同; Queries 不自建第二份 native 绑定缓存. 这防止把客户端输出指针按 query-buffer offset 解释. 只在 query 冷路径建立所需状态, 不在 Draw 热路径 glGet 保存/恢复.

### 4. 配对、结果与原 recording 关联, 复用不遗忘旧使用

Timer 的私有阶段区分 Idle、Begun、Ended、Ready 和无有效结果的失败/放弃, 保存本次 recordingId、最近使用的 recordingId 和缓存结果. End 只接受同一 recording 的 Begun. 即使 Reset/Get 已消费结果, 也不允许同一 query 在同一 recording 再次 Begin; 另一次 recording 只在前次已解析并成功复位/消费后复用.

只有 Ended/Ready 表示存在完整计时区间. 没有 Begin 的 End、重复 Begin/End、跨 recording 配对、未完成的覆盖/Reset 都在新时间戳之前拒绝, 保留已发点与所有者. Close 若还存在未配对 Begin, 明确拒绝正常成功提交; 不自动补 End 来制造耗时. abandon 或后续 recording 失败使该次未交付结果失效, 包仍按原协议保护已发点. owner 在 Close/收取 orphan/退休等既有阶段收拢 query 状态, worker Release 仍只执行原子交接.

关联信息使用同一稳定物理包及 recording 身份, 不让 query 强持列表/包而形成循环. 旧包收拢 query 时须核对使用身份, 不能覆盖该 wrapper 后来一次合法使用的状态. 失败/放弃的点在原包证明退休后才可 Reset 为可用; 不在不能完成的对象上无限 Pending. 仅 native timestamp 已 available 不授权退休整个 Draw 包或复用上传区段.

### 5. Poll 只取 available 结果, 显式取值才允许有限等待

PollTimer 检查本次两个点的 available, 不可用则立即 false, 不读取会等待的 QUERY_RESULT, 不返回伪 0 ms. 两点可用才取完整 GPU 数值并缓存秒结果; 在明确的 Reset/下一次合法使用前保留可读结果, 有效零时间仍是合法结果.

正常消费者先 Poll, true 后 Get. 保留公共 Get 可显式获取结果的用途: 尚未 ready 的完整区间允许调用者选择冷路径等待, 一次 flush 推进已发工作并以同一五秒截止点反复检查 available; 不用 glFinish/WaitForIdle, 不无限忙等. 未配对、失败或超时通过既有诊断返回公共失败占位值, 不缓存为 Ready 或改写提交完成值; 超时保留未证明的使用.

这是在固定源 Get 等待行为上增加既有项目的期限/失败边界, 不把等待扩散到 Poll、Draw、普通上传或默认帧流程. 后续 FrameSlot 采用有界轮转, 不在统计面板关闭时强读结果.

### 6. 新叶资源沿用 lease、预算与预分配删除

Event 是有 Device 引用的 CPU query wrapper, 不为每个 Event 分配 GLsync. Timer 是持有 Device 的叶资源, Begin 前纳入现有 UploadResourceLease/InternalOwnership; caller Release 不得删除仍被包使用的名字, query 的存在也不能被误记为真正外部 owner.

首个 profile 给实际存活/待删除的 Timer 原生名字规定有限预算: 512 个名字, 即 256 对. 创建整对前预检预算并准备 wrapper/两个删除节点, 任一步失败释放未发布状态并保留首错. 预算直到真正 native 删除或既定失效退出才归还, 不在 wrapper final Release 入队时假定名字已回收. 不建额外跨帧 query pool/历史缓存, 不每个 Draw 分配 query; 调用者持有少量对象轮转.

扩充现有 NativeKind/删除 switch, final Release 只交出已准备的节点, worker 不调用 GL 或申请退休存储. 正常删除仍在 owning/current context, 关闭预检仍拒绝外部 Event/Timer/列表/Device owner. 仅内部包引用可以正常 drain. Context 不可用时不再发 query/poll/delete, 不等待失败 ID, 由现有丢失环境路径清理 CPU 状态并交 Host 销毁确切 context.

### 7. 验收完整 Draw 消费链, 不改造普通上传为 volatile 系统

在同一套正式 Shader/RHI 输入中先上传标记 A→Draw→提交 A, 再在 A 未被回收时以普通更新写标记 B→Draw→提交 B. 较大 Buffer 更新和纹理 PBO 路径的包内区段不得覆盖 A 的源存储, 由真实完成后两份输出分别保留 A/B 证明顺序和内容. 对象跨包/复合依赖继续精确计账, trackLiveness=false 继续由 caller 保活.

读回只保留必要的输出 owner, 不把它说成“所有资源均只由内部保活”; Pipeline/集合及其输入依赖可另行 Release, 以 native 存活/删除和关闭预检观察其退休. 绘制结果复用 T03 已知标记, 不再证明所有 raster、格式、Shader lowering 或数值组合. Volatile/push 存储仍明确 Unsupported.

本包还核对完整批预检、Close 后提交、重复提交/委托失败、正常在途关闭、未提交 issued prefix、fence/flush/poll 失败及 context-unavailable 退出. 先复用未变合同的既有证据, 只为新查询和绘制扩展补足有区分力的观察. 这完成 T04 的执行/寿命目标, 不完成 T05 的可复用稳定图像门槛.

### 8. 沿已有文件职责接入, 文档与普通构建独立于 fixture

新增必要的私有 `Queries.hpp/.cpp` 和 `CommandListQueries.cpp`, 分别承接 query 对象/Device 方法与两个 recording 时间戳入口; 从 Unsupported 文件移除仅本次交付的方法. Backend/Device/ContextState/Submission/Ownership 仅补实际集成点, 不往 CommandList.cpp 堆入全部查询实现.

公共/Validation 签名、Shader profile/包 ABI、Generic RHI 的依赖方向不变. 原生类型检查与配对状态由 owning backend 负责, Validation 不缓存另一份 query 阶段. CMake 显式列正式文件, RHI/Build 说明只更新真实支持范围. 中文职责/合同/流程注释及上游许可保留, B 通读实际涉及的完整文件.

开发材料只写入 `build/d03-t04/`. 当前存在的 `build/d03-t03/fixture` 可作本地输入来源, 它不是永久工程前提; 若已清理, 不自动恢复测试目录. 所需最小新输入仍使用正式 Shader 编译/装配链, 不加入正式模块、Shaders 或默认构建.

## Risks / Trade-offs

- [Event 错等当前队列尾或将零 ID 当未提交完成] → 捕获/等待身份独立于以后提交, 明确零 ID 语义, 检查两个提交边界及未提交前缀.
- [query 可用、提交完成和 CPU 释放被混为一事] → 两点可用只产生计时结果, 上传/复合依赖仍由现有真实完成协议退休.
- [Reset/旧包退休覆盖后来 query 使用] → 配对和收拢核对 recording 身份, 未解析的名字不覆盖, 不引入无界历史版本.
- [快速 GPU 导致看不到 Pending] → 不靠增加负载制造时序; 实际结果证明 Complete, 用 build 内受控 native 返回单独证明 Pending/Failed 映射, 不称自然驱动故障.
- [取计时值意外阻塞普通帧] → Poll 先查 available, 显式 Get 等待限定冷路径/单次期限, 正常验证路径先 poll 后 Get.
- [验证膨胀或沿用过期二进制] → 按 tasks 的分组预算复用输入, 保留源码/fixture/构建/exe 身份和首错, 不重跑未变矩阵或用失败构建后的旧 exe 充数.

## Migration Plan

1. 在现有 owner 下接入新 query 对象/入口, 完成最小正向查询链后落实复用、删除、失败和 Draw 在途集成, 保留未交付命令的拒绝.
2. 按 tasks 的预算记录实际原生、受控失败、复用及未覆盖证据, 维护者调度独立 B 审查/注释, A 整改后由 B 复核最终版本.
3. B 验收后才最终 verify, 留存 CR/注释覆盖/最终输入, 同步本 change 的两份 delta 并归档; 归档成功后按模板重置当前问题单, 开发计划只更新状态/引用.
4. 无数据格式迁移或部署. 若撤销本包, 依据本 change 实际 diff 恢复此前查询明确 Unsupported 的状态, 保留原上传/Draw 协议和历史记录, 不留下可调用半实现或伪完成入口.
