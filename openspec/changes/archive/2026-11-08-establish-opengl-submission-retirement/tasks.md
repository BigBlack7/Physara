# Tasks

范围为 [D03-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t04), 依据 [design](design.md)、[查询规范](specs/opengl-completion-queries/spec.md) 及 [能力边界 delta](specs/opengl-device-resources/spec.md). 以下分别跟踪 A 实施、独立 B 审查/复核和最终收尾, 规划文档通过不代表实现或独立验收完成.

## 1. 查询对象与原生存储

- [x] 1.1 A 在现有 OpenGL Device 下接入必要 Queries 文件、显式 CMake 源清单和真实 Event/Timer 工厂, 核对 entry points、实际 timestamp counter bits/有效范围, 实现整对计时名字的有限预算及发布前事务准备; 补齐对象/能力/失败注释, 编译受影响 RHI 目标, 按 V1 观察最后合法容量、紧邻超额拒绝及实际删除后的恢复, 不返回半成品对象.
- [x] 1.2 A 将 Timer 叶资源、预分配删除节点、NativeKind 和 Device 引用纳入现有 lease/InternalOwnership/删除协议, Event 只保留观察者所需所有权; 按 V1/V5 核对 caller/worker Release、未删除名字仍占预算及 owner-context 最终删除, 无新增强引用环、退休分配或第二条保活队列.

## 2. 已接受提交的 Event 快照

- [x] 2.1 A 实现 Set/Reset 和零快照语义, 沿现有公共边界拒绝错误实现/Device/队列/调用环境, 保留非法重设前的快照; 将“零 ID 不证明未提交工作完成”和“Reset 不取消 GPU 工作”写入接口/实现说明, 按 V2 检查空历史、A 后设置再发 B、Pending Reset 三种不同语义.
- [x] 2.2 A 扩展既有 collector 的截止身份, 以同一完成权威实现 Event 非阻塞 Poll 和单次五秒预算的显式 Wait; 按 V2 用实际 fence 证明完成, 用必要受控返回区分 Pending/Failed/timeout, 观察 Wait(A) 在 A 完成后不等待 B, 重复观察不重复退休, 不调用等待新队尾的 WaitForIdle.

## 3. 即时 GPU Timer 区间

- [x] 3.1 A 实现同 recording 的 Begin/End 时间戳对、发出前引用/issuedWork 和不同查询的重叠区间, 拒绝无配对、跨 recording 及同 query 同 recording 复用; 写清配对合同, 按 V3 在真实 Draw 周围观察每点仅发一次、Execute 不重放及一个代表性非法配对.
- [x] 3.2 A 实现 available-first Poll、完整 GPU 数值到秒的转换/缓存和显式 Get 的有限等待, 客户端结果读取通过唯一 ContextState 建立 query-buffer 解绑状态; 按 V3 独立比较原生时间点及单位, 证明 Pending Poll 不读阻塞结果、合法零值可区分、显式 Get 到期不缓存 Ready 或推进提交完成值, 注释有效 counter 范围及冷路径等待边界.
- [x] 3.3 A 落实 Reset/消费后跨 recording 复用、旧包身份核对、未配对 Close 与失败/abandon 状态收拢; 按 V3/V5 检查 Pending 覆盖被拒绝、放弃使用退休前不可恢复、合法新使用不被旧包收拢覆盖, 已发前缀不因结果失效而提前释放, 同步说明各阶段和 owner 交接.

## 4. 完整 Draw 在途与退出链

- [x] 4.1 A 沿用既有提交/上传/图形依赖权威, 按 V4 验收 A/B 两次真实 Draw、后续普通 Buffer/PBO 更新和共享复合依赖, 只修正实际暴露的缺口; 由独立预期的两份输出、native 生存/删除及实际完成证明源区段不覆盖、输入依赖不提前回收, 明确保留读回所需输出 owner, 不扩张为 volatile/push 版本系统.
- [x] 4.2 A 核对真实 Draw/查询链上的全批预检、Close 后提交、重复执行和失败前缀, 按 V2/V4 复用匹配的既有证据并补必要观察; fence/flush/poll 失败不返回成功票据或假完成, tracker metadata 仅在接受后发布, 原始失败和未证明的引用保留, 相关流程注释与实际顺序一致.
- [x] 4.3 A 将查询纳入正常在途 drain、未提交 issued prefix/orphan 和外部 owner 关闭预检, 按 V5 观察内部引用可收拢、真实外部 query owner 可重试阻止关闭、释放后可继续退出, worker 最后 Release 仅原子交接; 记录每个关闭状态的实际所有者与重试条件.
- [x] 4.4 A 按 V5 核对查询/Draw 参与的 Pending 关闭期限、失败 fence/无 fence 前缀及 context-unavailable 退出, 仅修正本次集成缺口; 到期仍保留 Device/context 与工作, 可用 context 的退出兜底不进入正常帧, 已确认失效后不再 poll/delete GL 或等待失败 ID, 不把受控错误称为自然设备丢失.

## 5. 公共边界与正式工程集成

- [x] 5.1 A 用 V2/V3/V4 的共享输入检查 raw 与 Validation 的必要拒绝边界, Validation 保持委托且不另存 query 阶段; 用一项 foreign/归属错误和一项查询状态错误区分真实风险, 按实际改动补充尚缺的独立失败, Debug/Release 均不依赖 assert 才拒绝, 不建立逐方法乘逐参数矩阵.
- [x] 5.2 A 只从 Unsupported 移走已交付 Event/Timer 方法, 更新受影响 RHI/Build 能力说明及完整职责/合同/流程注释; 按 V6 核对公共 ABI、Shader profile、Generic 依赖和第三方不变, 文档链接/能力描述匹配实现, 在无 fixture 的新 binary tree 配置并构建 Debug/Release 正式受影响目标及实际消费者.
- [x] 5.3 A 汇总各组实际检查并以共享真实 Draw 输入完成 V2/V3/V4 的联通检查, 保留匹配输入的构建成功与 exe 身份、设备/驱动、实际命令/结果及未覆盖项; 证据满足预算即停止, 不重跑 T03 全矩阵, 不将 query Ready、CPU 检查或过期 exe 结果称为完整在途验收.

## 6. A 开发交接

- [x] 6.1 A 完成上述实现/本地检查后, 在本文件记录基准 branch/HEAD、相关暂存/未暂存 diff 和未跟踪文件、必要 build 输入/命令/结果/限制, 提供实际 fixture 供维护者交接; 开发计划标为待独立审查并引用本 change, B 的任务保持未勾选, 当前问题单由本轮审查使用.

## 7. 独立审查、整改与复核

- [x] 7.1 B 独立核对实际版本、合同、diff 和直接消费者, 通读涉及的完整第一方文件并补齐职责、接口合同和实现流程注释; 在 [CodeReview.md](../../../../Docs/CodeReview.md) 按稳定 CR 编号记录问题、注释覆盖及证据范围, 由 B 勾选本项.
- [x] 7.2 A 按 CR 编号整改并完成受影响自检, 保留 B 注释并同步核对整改后的说明, 在本文件记录修正/验证摘要; 无正式问题时注明不适用原因, 只由 A 勾选本项, 不代替 B 验收.
- [x] 7.3 B 复核最终输入、各 CR 修正与匹配证据, 完善受整改影响的完整文件注释, 分别确认代码正确性和注释可读性; 只有正式问题闭环且必要验收无缺口才由 B 勾选, 否则保留问题单并交回 A.

## 8. 最终验证、同步与归档

- [x] 8.1 A 在 B 验收最终版本后执行原生 OpenSpec verify 工作流及其必要检查, 核对实现与全部 delta/复用合同、任务和证据一致, 严格结构验证通过; 若代码/构建输入再变则重新交 B 复核受影响范围, 不将 CLI 计数等同独立验收.
- [x] 8.2 A 在本文件留存最终版本、CR 修正/复核摘要、B 注释覆盖、实际运行/复用/未覆盖证据, 沿原生 sync 流程仅同步本 change 两份 delta 并严格验证主 specs; 核对原有合同/场景被保留, 不扩大 T05、读回或呈现声明.
- [x] 8.3 A 在最终 verify、同步与独立闭环均成功后按原生流程归档, 确认归档路径与内容再据模板重置当前问题单; 开发计划只更新 D03-T04 完成状态和归档引用, T05/呈现/D03 阶段出口仍待各自验收, 归档失败保留现场.

## 验证预算与停止条件

实现影响 L3 GPU 寿命/同步和 L2 公共查询语义. 本次规划文件本身仅作 L0 内容、链接与原生结构检查. 各实现组当场完成自己引入的最小检查, 第 5 组只补跨组联通与正式消费者证据, 不把早期验证推迟到收尾.

| 组 | 具体风险与最小有效观察 | 复用与停止条件 |
|---|---|---|
| V1 对象/预算 | 真实创建后检查实际 counter bits/范围; 观察最后合法计时对、紧邻超额拒绝及 native 真正删除后的预算恢复. 只在新增创建事务缺少证据时补一个准备/原生失败, 确认首错和未发布状态回收. | 复用既有删除节点/回调/可传播分配失败合同, 不逐分配点扫描或制造自然 OOM. 容量与删除归还各有区分力即可. |
| V2 Event/完成 | 在同一 A/B Draw 链上捕获 A 再发 B, 实际 fence 证明 Complete; 空快照、Reset 只观察者及 Wait 截止身份各有观察. 用 build 内受控 native 返回证明未自然出现的 Pending、Failed/timeout, 观察引用/ID 不假推进. | 快速 GPU 不要求自然 Pending, 不增加负载制造时序; 对 unchanged preflight/完成合同引用输入匹配的既有结果. 不以计时耗时阈值推测 Wait 是否等了 B. |
| V3 Timer | 两个重叠时间戳区间覆盖同一真实 Draw, 原生完整时间点及纳秒单位独立证明公开秒结果. 一次 available=false 观察足以证明 Poll 不读结果; 合法复用、Pending Reset/非法配对、旧包收拢及显式读取到期分别覆盖其不同状态风险, 可共用一次录制序列. | 不要求已知固定 GPU 耗时, 不复制私有换算实现当预期; 仅在实际位宽下验证合法测量范围. 不为每个入口重复所有非法参数或配置. |
| V4 Draw/上传/拒绝 | 复用 T03 正式 Shader/RHI 标记输入, 两次提交分别写 A/B 并输出至可独立观察的目标. 后续较大 Buffer 更新/PBO 上传不覆盖未退休源区段; 释放输入 Pipeline/集合/资源 owner, 保留读回所需输出 owner, 分别观察像素/数据及 native 退休. 新 public 边界只补能触发不同缺陷的代表性拒绝. | 重用当前仍存在且输入匹配的 fixture/日志; 本地材料缺失不自动恢复. 不重证 raster/格式/lowering 矩阵或 T05 稳定图像门槛, 不用仍持有的输出证明全部资源只靠内部保活. |
| V5 退出/失效 | 共享上述链观察正常内部 drain、外部 query owner 阻止关闭后释放重试、worker 最后 Release 的已发前缀. 对本次受影响的 pending 到期、不可用 fence/flush/poll 与 context-unavailable 路径, 复用匹配证据或补最少受控返回, 分别核对保留、可用 context 退出收拢和失效后无 GL 调用. | 受控 native 返回只称受控失败; 不宣称复现自然驱动错误/设备丢失. 已覆盖同一状态映射即不逐错误码、对象种类或 shutdown 入口扩展矩阵. |
| V6 构建/证据 | 正式受影响目标及实际消费者 Debug/Release 构建, 主要原生链在 Release, Debug 一次代表性联通. 在无 fixture 的新 binary tree 证明普通构建仅依赖仓库输入, 无默认测试目标/下载/恢复; 检查文档及实际输入/build/exe 对应关系. | 不删除现有 build. 只有公共/Validation/共享构建闭包实际变化才扩大 Generic/消费者范围. 同输入成功后不重复完整构建; 保留 MSVC C1001 首次失败和原选项, 失败构建后的旧 exe 不作成功证据. |

所有新开发代码、Shader/schema、脚本、受控 native 替换和临时工程只写入 `build/d03-t04/`, 不进入正式源码、默认构建、版本控制或永久生产测试开关. 需要时只在 fixture 中替换已加载 GLAD 函数入口以观察真实 backend 的边界, 不为测试引入正式故障接口. 保留实际命令和输入描述, 不假定任何本地 build 文件永久存在.

达到本包要求、已识别风险有相应证据且输入匹配后停止. 仅失败、输入变化或发现具体未覆盖缺陷时扩大检查. 不纳入性能比较、跨 GPU/OS 矩阵、自然致命 OOM 或后续公共异步读回/Renderer/Editor/呈现验收. 必要环境或证据缺失时保留相关任务未完成, 不用结构校验替代运行验收.

## 实施与验收记录

2026-11-07: 规划起点 `v2.0` / `6d4dc33107b1b523adbd100846fa1217b7adb9a4`, 创建 change 前工作区干净. 当前仅形成 proposal/design/两份 delta/tasks, 以上 21 项均待实施, 未运行 C++ 构建或原生 GPU 验收, 未启动本轮独立 B 审查. 此处将在 apply/交接时按实际输入补充证据, 不能以规划基准替代最终交接版本.

规划检查: `openspec validate establish-opengl-submission-retirement --strict --json` 通过; 7 份相关 Markdown 的 160 处本地链接/锚点、8 项要求/28 个场景、被修改要求的既有场景标题保留及任务编号/未勾选状态通过本地 L0 核对. 原生 `instructions apply` 返回 ready, complete=0/total=21. 以上仅证明规划结构与引用一致, 不属于实现、GPU 或 B 验收证据.

### 2026-11-08 A 实施与 Release 原生检查

实施起点仍为 `v2.0` / `6d4dc33107b1b523adbd100846fa1217b7adb9a4`; 进入 apply 时保留上一轮两份开发计划修改和本 change 的未跟踪规划文件, C++/CMake 无既有未提交修改. A 未暂存、提交或推送. 实现使用现有 Device/packet/lease/FIFO, 新增 Queries.hpp/.cpp 与 CommandListQueries.cpp, Query 删除和客户端取值状态仍归原 owner. 公共 RHI/Shader ABI 与第三方输入未改.

- `cmake --build build --config Release --target PhysaraRHIOpenGL --parallel 4` 原生构建通过, 日志 `build/d03-t04/build-rhi-release-native.log`. 首次沙箱构建因 Windows SDK 用户目录访问被拒绝而失败, 保留 `build-rhi-release.log`, 正常权限重试未更改编译选项.
- 本地工程由 `cmake -S build/d03-t04/fixture -B build/d03-t04/native -G "Visual Studio 17 2022" -A x64` 配置. `python build/d03-t04/Evidence.py build Release` 构建成功后, `python build/d03-t04/Evidence.py run Release positive create timestamp result fence flush wait-failed loss unavailable pending` 全部通过. 只有匹配源码/fixture、CMakeCache、实际 Shader 包和 exe 的成功构建才允许 run; 构建快照、各运行输入/退出码及日志身份分别在 `build-Release.json`、`runs.jsonl` 和 `run-Release-*.log`.
- 实际环境为 Windows x64/MSVC 19.44.35228、SDK 10.0.26100.0, Intel RaptorLake-S Mobile Graphics Controller, OpenGL `4.6.0 - Build 32.0.101.6129`. 原生 timestamp 有效位为 64, 一个 counter 周期为 `2^64` 纳秒, 本次短区间在有效范围内. 结果来自驱动的两个完整时间点, 不是 CPU 计时性能结论.
- V1 验证 256 对最后合法容量、紧邻拒绝、worker 释放后待删预算仍占用及真正删除后恢复. `create` 在真实 Draw 前缀后受控制造部分名字/错误, 观察已取得名字回滚及首错保留. 未注入每个 CPU 分配点或实际致命 OOM.
- V2/V3/V4 共享同一正式 schema→canonical 包→RHI 绑定/Draw 链: 两份已知输出分别读取 A/B 的 512 KiB 普通 Buffer 更新和 PBO 上传, CPU 源当场销毁; 输入 Pipeline/集合/资源 owner 释放后仍保活, 只保留读回所需输出 owner. 可用 Timer 不使 Pending 包退休; Wait(A) 只调用 A fence, A/B 源区段分别退休. 已解析 query 在旧 A 未退休时进入新 recording, 旧包收拢不破坏后继 Begin/End. 另覆盖重叠 Timer、单位/稳定缓存、query-buffer 解绑、未解析 Reset/覆盖、同 recording 重复 Begin、原生/Validation 预检拒绝和重复提交, Execute 未重放点或 Draw.
- V5 覆盖未配对 Close、worker 交接未提交已发前缀、失败区间退休后恢复、真实外部 Event owner 拒绝关闭后释放重试, 以及仅内部 Draw/query 依赖的最终 drain. 受控 `timestamp/result/fence/flush/wait-failed` 分别观察失败点不伪成功并由可用 context 只在退出收拢; `pending` 验证 Event/Get/关闭的单次期限和关闭保留后重试; `loss/unavailable` 验证停止后无 query/result/wait/delete GL 调用及不虚构完成. 它们是受控 native 返回, 未声称复现自然设备丢失.
- 首轮本地输入修正包括 program 必填 `defines`、fixture 的 Shader 名称歧义、资源 initialState 初始化以及故障退出预期须保留既有首错; 失败构建日志与 `runs.jsonl` 的失败退出记录保留, 同名运行日志记录最终一轮, 早期 fixture 失败不计成功证据. 全部开发代码/Shader/故障入口只在 `build/d03-t04/`, 正式代码未加入测试宏或默认样例.

上述证据满足 1.1–4.4, 当前只勾选 A 已完成项. Debug、正式新 binary tree 的双配置消费者及最终交接仍在推进; B 审查/整改复核、最终 verify、sync/archive 均未执行.

### A 本地验收完成与 B 交接

2026-11-08: A 的 1.1–6.1 共 15 项完成, 7.1–8.3 的独立审查、整改/复核与最终收尾 6 项保留未勾选. 维护者调度 B, 本会话未替 B 审查或验收, 工作包整体尚未完成.

| 最终输入/产物 | 身份或检查 |
|---|---|
| branch / HEAD | `v2.0` / `6d4dc33107b1b523adbd100846fa1217b7adb9a4`, 以本地实际未暂存 diff、新增文件共同构成交接版本, 未提交/推送 |
| 源码与 fixture 联合输入 | `941aef7662438bf2cc2ca1e84b2675f69722226787afca052100835bd4336719`, Debug/Release 构建记录相同 |
| Debug SubmissionChecks.exe | SHA256 `ba26e949ba7544aaac0808623807ff9839e9e9b0858594ac1d43b333dd1615b5`, `python build/d03-t04/Evidence.py run Debug positive` 通过 |
| Release SubmissionChecks.exe | SHA256 `ae45426aa18c45b996699cff1a8e054163df14344a3c7634950831deb1c3928c`, positive 及上述 9 个受控失败模式均通过 |
| 当前构建与运行证据 | `build/d03-t04/build-Debug.json`, `build-Release.json`, `build-current-*.log`, `runs.jsonl`, `run-*.log`; 含实际源码/fixture 文件哈希、CMakeCache/Shader 包/exe 身份, 只有成功构建允许 run |
| 无 fixture 正式工程 | `cmake -S . -B build/d03-t04/formal -G "Visual Studio 17 2022" -A x64`, 两配置 `cmake --build build/d03-t04/formal --config <Debug/Release> --target Physara --parallel 4` 通过, 各自 `Physara.exe --hidden --poll-limit=3` 退出码均为 0 |
| 正式构建/运行日志 | `configure-formal.log`, `build-formal-final-Debug.log`, `build-formal-final-Release.log`, `run-formal-Debug.log`, `run-formal-Release.log`; 该 binary tree 无 SubmissionChecks 或 fixture 项目 |
| 实际交接包 | `build/d03-t04/handoff-fixture.zip` 包含本轮最小检查输入与本地执行/记录脚本; `handoff-inputs.json` 含实际 diff/新增文件、源输入及产物身份, 不把这些临时文件当作永久工程前提 |

补齐 Queries.hpp 的直接标准库 include 后, 首轮 Debug 虽编译成功但输入守卫检测到构建期间源码变化, 未运行该 exe; 日志另存 `build-first-Debug-input-changed.log`. 最终两配置均重新取得上述一致输入的成功构建, 无降低警告、优化或 Debug STL 检查. 本轮未观察到 C1001; 历史原因仍不作推断. 正式根、公共 RHI、Validation、Shader ABI 及第三方未因查询接入改变依赖; 仅原生 CMake 显式增加正式文件, 无默认开发目标或测试开关.

验证边界: 本轮实际运行 V1–V6 所述查询/完整 Draw 在途消费者, 没有用 T03 的旧 exe 或旧日志代替当前检查. T03 的未变 raster/格式/Shader lowering 合同只沿用其归档与源码依据, 本轮不重新宣称其完整矩阵运行. 原生环境仅上述 Intel/Windows, 自然设备丢失、致命 noexcept OOM、跨 GPU/OS、性能比较、counter 多周期长时间测量、T05 稳定图像与后续公共异步读回/Renderer/Editor/呈现均未验证. 有效零耗时由 available/Ready 与独立失败状态区分, 未宣称自然取得精确零耗时样本.

待审范围为本 change 实际新增/修改的第一方 OpenGL C++、原生 CMake、说明和规划, 完整文件及直接消费者按 [独立审查技能](../../../../.agents/skills/physara-review/SKILL.md) 核对. 当前 [CodeReview.md](../../../../Docs/CodeReview.md) 只填 A 的交接信息, B 问题、注释覆盖、代码/可读性结论均待其独立填写. 最终 verify、主 specs 同步及归档尚未进行.

A 文档自检: change strict 验证通过, 10 份相关 Markdown 的 196 处本地链接/锚点及 `git diff --check` 通过; 8 项要求/28 个场景及被修改要求的旧场景标题保持完整. 实际待审内容为 26 份文件, 以最终 `handoff-inputs.json` 列表和逐文件哈希为准. 达到本包本地验收预算后停止追加行为检查, 留待 B 按实际影响独立审查.

### A 无功能整改项与最终复核交接

2026-11-08: 按当前问题单处理 7.2. B 的第 1 轮源码审查未发现 P0–P3 正式缺陷, 未分配 CR 编号, 功能整改不适用. B 已通读并补齐完整 14 份第一方 C++ 文件及 1 份原生 CMake 的注释, 其代码与注释结论、完整覆盖和限制仍保留在问题单. A 本轮未修改源码、接口、构建输入或 B 注释, 只更新本交接及阶段状态.

- B 进入前的 15 文件集合摘要为 `7d57149edc057fa3747a6a9c045f1e431d6a71bc8a4176857d28c7de1bf50c99`, B 注释后的集合摘要为 `ca3fc3d66dcc64f4dffd6495bd503528a69daa6d853e4dd263ea60fe45f91225`. A 从原交接逐文件哈希和当前实际字节分别重算, 与问题单两项均一致; 摘要算法沿问题单的相对路径排序和 `path + TAB + SHA256 + LF`.
- `python build/d03-t04/CheckReviewInputs.py` 通过. 其余 1053 份代码输入、6 份 fixture 及 12 份既有产物与原 A 交接一致, 15 份受注释编辑文件未发现行尾反斜杠续行. 逐文件身份与检查结果保存在 `build/d03-t04/a-correction-inputs.json`; 原 `handoff-inputs.json` 保留为 B 进入前的基准, 不将它覆盖成已验证 B 新字节的构建记录.
- 复用 B 已记录的 15/15 去注释/空白后内容相同及 include/预处理指令检查. A 核对新增查询/计时入口的职责、状态和失败说明, 按注释改动的 L0 范围进行 diff、文档与链接自检, 不新增行为用例或重复 GPU 矩阵.
- 上一节的 Debug/Release 构建、原生运行与受控失败是其原联合输入 `941aef7662438bf2cc2ca1e84b2675f69722226787afca052100835bd4336719` 的证据; B 注释改变了源码字节, 本轮没有运行旧 exe, 也未把旧哈希或运行结果描述为 B 最终版本的新构建/独立复现. 后续最终 verify 的检查范围在 B 最终复核后确定.

当前仅勾选 A 的 7.2, 计数为 16/21. B 已交付 7.1 的初审报告, 但按其受限编辑范围未勾选 tasks; 7.1/7.3 的勾选仍由 B 负责. 当前问题单明确最终复核尚未执行, 故交接 B 确认本节最终输入与注释, 分别签署代码正确性和注释可读性. 最终 verify、sync/archive 和问题单重置仍待闭环条件满足, 本阶段清单保持未验收.

A 本轮自检结果: review 输入核对、`git diff --check`、10 份文档的 223 处本地链接/锚点及 change strict 结构校验通过. `a-correction-inputs.json` 另存当前 26 份交接内容的逐文件哈希, 与原 A 构建记录分开, 供 B 对照本次最终复核输入.

<a id="b-final-review"></a>
### B 最终复核与注释验收

2026-11-08: 维护者在 A 完成 7.2 后调度原独立 B 会话复核. B 已自行勾选 7.1/7.3, 保留 A 的 7.2, 当前为 18/21; 最终 verify、同步与归档三项仍归 A. 本节为归档前的审查留存, 不依赖未来重置后的当前问题单才能理解结论.

| 项目 | B 实际确认 |
|---|---|
| 最终版本 | branch `v2.0`, HEAD `6d4dc33107b1b523adbd100846fa1217b7adb9a4`, 加实际相关未暂存/未跟踪文件. 15 份源码/CMake 的原始字节与 B 初审结束时逐文件相同, 集合 SHA256 仍为 `ca3fc3d66dcc64f4dffd6495bd503528a69daa6d853e4dd263ea60fe45f91225` |
| 摘要口径 | 仓库相对路径按字典序排序, 各项 `path + TAB + SHA256(文件原始字节) + LF` 拼接为 UTF-8 后计算 SHA256. 覆盖文件见下文; 不将此 15 文件摘要冒充源码/fixture 联合输入或完整工作区摘要 |
| 正式发现/整改 | 首轮及最终复核均未发现可确认的 P0-P3 缺陷, 未分配 CR 编号, 无未关闭或撤回条目. A 的功能整改不适用, 其 7.2 对未变代码的自检与本次实际输入一致 |
| 代码正确性 | B 通过. 首轮通读实际完整文件/合同和直接消费者, 本次核对不变身份并再次检查 Event 捕获/截止身份、Timer 配对/结果/复用、Close/orphan/退休及删除/退出集成, 未发现新缺陷 |
| 注释可读性 | B 通过. 首轮补齐职责、接口前提/失败结果及阶段不变量, 此后 15/15 文件无字节变化, 本次无需修改源码注释. 首轮已证明保留字面量并剔除注释/规范化空白后的内容 15/15 一致, include/预处理及构建关系未变 |
| 本轮实际检查 | 15 文件字节/摘要核对、相关 Git diff 范围检查、签署前 10 份 Markdown 的 223 处本地链接/锚点通过. 签署后同 10 份文档的 224 处链接/锚点、源码摘要复查及 `git diff --check` 通过. OpenSpec `1.13.2` 的 `validate establish-opengl-submission-retirement --strict --json` 为 1/1 valid、无 issues; `list --json` 确认 nearest root 为 `D:\MyWorkSpace\Physara`, 本 change 为 18/21, 仅 8.1-8.3 待完成. 结构检查不替代最终 verify |

完整注释覆盖为 14 份 C++ 与 1 份 CMake, 路径相对 `RHI/OpenGL/`:

- `Queries.hpp`, `Queries.cpp`, `CommandListQueries.cpp`: Event 零快照/Reset、Timer 阶段/配对/Ready 与失败复用、GPU 数值单位/有效周期、available-first 与显式等待、预分配删除和旧包身份保护.
- `Backend.hpp`, `CommandList.hpp`, `CommandList.cpp`, `CommandListOwnership.cpp`: Device/列表/资源职责、公开入口及失败合同、唯一 claim、未配对 Close、叶对象与复合强引用的实际记账. 简单访问器/薄转发共用已有说明, 上游许可保留.
- `Submission.hpp`, `Submission.cpp`: 稳定物理包/FIFO 与 recording/submission/completed 身份、结果与包完成分离、查询先断开借用再退休、orphan 已发前缀和受控退出.
- `ContextState.hpp`, `ContextState.cpp`, `Device.cpp`: 唯一 native 缓存与 query-buffer 指针语义、失效/错误检查责任、首错及 context 健康、发布前回滚/发布后 worker 入队、实际删除后归还预算.
- `Unsupported.cpp`, `CommandListUnsupported.cpp`, `CMakeLists.txt`: 剩余拒绝表的共享失败合同、已交付查询的实际归属、正式源清单及 PUBLIC 通用 RHI/PRIVATE native 依赖边界.

直接消费者复核覆盖公共 `RHI.hpp` 查询合同、Validation 查询/列表/提交委托, `UploadResourceLease` 与 Draw/图形用途/普通 Buffer/PBO 上传的相关路径, `OpenGL.hpp` 和 `Platform/RhiDeviceHost.cpp` 的关闭前提, 根/原生 CMake、Build/RHI 能力说明、Runtime 实际入口. 架构依据保持 7.4/7.6/11.1/11.3/11.5, 本 change 的 8 项要求/28 个场景及复用的上传/即时绘制合同未发生功能调整.

运行证据处理: B 按维护者范围没有扫描或读取 build, 没有重跑构建、GPU 或受控失败. A 已记录的 V1-V6 是原联合输入 `941aef7662438bf2cc2ca1e84b2675f69722226787afca052100835bd4336719` 的实际运行; B 的注释前后内容检查与此次字节一致性证明后续仅为 L0 变化, 因而复用这些未变行为的结果, 不新增用例或重复矩阵. A 对其余 1053 份代码、6 份 fixture 和 12 份产物的身份检查仍标为 A 记录, B 不声明独立核验该联合哈希或 exe. A 的 26 文件交接记录也不冒充本次签署后文档的新字节身份.

证据限制保持原样: 仅已记录 Intel/Windows 环境, 受控 loss 不等于自然设备丢失, 未验证跨 GPU/OS、致命 noexcept OOM、性能或多 counter 周期计时. T05 稳定图像、公共 staging/异步读回、Renderer/Editor、呈现和 D03 阶段出口继续保留后续验收. 本次只改文档, 未引入需补原生观察的新行为风险.

B 结论: 代码正确性与注释可读性均通过, A/B 审查闭环完成, A 可开始 8.1 最终 verify, 然后按 8.2/8.3 同步与归档. B 本轮仅更新本节/所属勾选、当前问题单和开发计划状态, 不修改源码/构建输入/主 specs, 不执行 A 的最终 verify 或提前归档/重置问题单; 后续若代码或构建输入变化, 须再交 B 复核受影响范围.

<a id="final-verification"></a>
### A 最终 OpenSpec verify

2026-11-08: 使用仓库原生 `openspec-verify-change` 工作流, 读取 CLI 的 spec-driven 状态、apply contextFiles 和任务, 对 proposal/design/两份 delta/实际实现及上述 A/B 记录核对. 当前 root 为 `D:/MyWorkSpace/Physara`, 非外部 store. B 的最终代码集合摘要 `ca3fc3d66dcc64f4dffd6495bd503528a69daa6d853e4dd263ea60fe45f91225` 与实际 15 文件一致; 其余 1053 份代码、6 份 fixture 和 12 份原产物身份未变. 本轮未改 C++、Shader、CMake 或 B 注释.

| 维度 | 最终 verify 结果 |
|---|---|
| Completeness | 实现/本地验收及 A/B 审查 18/18 完成; 8 项 delta 要求均有实现/交付依据. 本次完成 8.1 后为 19/21, 仅规范同步 8.2 和归档后确认 8.3 待执行, 不以规划产物 done 冒充任务完成 |
| Correctness | 8/8 要求映射到实际实现及记录; 28/28 场景逐项核对 WHEN/THEN 与实现/既有检查依据, 按下表共享证据归并. 覆盖指代码与证据核对, 不声称每种非法输入变体都曾独立原生运行 |
| Coherence | design 的 8 项决定与实际 owner、FIFO、时间戳对、复用/退休、预算、上传边界和文件/CMake 职责一致. PascalCase 文件、中文合同/阶段注释、上游许可及单向依赖经完整 B 审查, 无新模式偏离 |

| 要求与场景数 | 实现/直接消费者证据 | 实际运行或复用依据 |
|---|---|---|
| Capture accepted graphics submission snapshots, 3 | `RHI/OpenGL/Queries.cpp:97/114/130/144`, Set 只取已接受 ID, 零快照/Reset 不触及提交 owner | V2 positive 的 A/B 截止快照、空历史/未提交前缀、Pending Reset; B 复核同一实现 |
| Resolve event snapshots through the existing completion authority, 3 | `Submission.cpp:255` 的 stopAt/FIFO 与 WaitSync, `Queries.cpp:114/130` 区分完成/Pending/超时/失败 | positive、pending、wait-failed/loss 的实际或受控完成观察, 无新队尾等待/假 ID |
| Record valid timer intervals in one immediate recording, 3 | `CommandListQueries.cpp:13/68`, `CommandList.cpp:160` 的 Close 与 `Queries.cpp:63` 的关联收拢 | 重叠真实 Draw 区间、非法重复/覆盖、未配对 Close、worker abandon 与恢复; Execute 不重放 |
| Return available GPU durations in public seconds, 3 | `Queries.cpp:221/271/279`, `ContextState.cpp:136`, available-first、实际位宽模差和秒缓存/有限 Get | positive 原生完整时间点/单位、受控 unavailable 和 pending/result; 合法零值状态与自然零样本限制已说明 |
| Bound and retire query ownership before destructive reuse, 4 | 工厂 `Queries.cpp:154`、Reset `:324`、Reconcile `:63`, 既有 RetainResource/RetirePacket/CheckShutdown/DeleteNative | V1 容量/worker/真正删除, 新 recording 先于旧包退休、外部 query owner、内部 drain 和 unavailable 退出 |
| Validate query inputs and preserve primary failures, 2 | OwnedQuery、Recording/owner/current 与整对工厂事务, 既有 NativeFailure/Report/预分配删除 | raw/Validation foreign/null/阶段等代表性拒绝及 create/timestamp/result 受控失败; 首错与已发前缀保留 |
| Reject unavailable operations without fictitious GPU work, 6 | `Unsupported.cpp`, `CommandListUnsupported.cpp`, 新 Queries 定义及公共/Validation 委托; RHI/Build 支持表 | 有限 Draw/查询实际运行及明确拒绝源码, 公共 staging/map/其它扩展仍拒绝; 没有从新查询推导读回或呈现支持 |
| Report allocation separately from completed rendering, 4 | 本 tasks 的范围/实际环境/限制、B 结论、RHI README 与开发计划工作包状态 | 分配/准备/Draw/在途及 T05/阶段出口各自记录; 旧 D01 条件场景按既有合同保留, 不把本次交付当作后续验收 |

验证执行边界: B 已证明 15/15 注释前后非注释内容一致, 最终又证明字节未再变化. 本轮按 `physara-verification` 的 L0 规则复用前述 V1–V6 的实际原生/受控失败结果及相应 Debug/Release 构建, 没有启动旧 exe、重跑完整矩阵或新增用例. 原运行输入仍标为 `941aef7662438bf2cc2ca1e84b2675f69722226787afca052100835bd4336719`, 不将它冒充注释后源码的新构建. 本轮身份核对 JSON 是可再生成的本地索引, 最终代码验收以 B 签署摘要为准, 新文档/主 specs 的身份另记收尾快照.

问题判定: 未发现缺失实现、未覆盖的范围内行为合同、design 偏离或代码模式问题; WARNING/SUGGESTION 为 0, 适用检查无跳过. 任务检查在进入 verify 时确有 8.1/8.2/8.3 三个未完成项; 本节完成 8.1, 余下两个属于仍须实际执行的收尾阻止项, 不提前宣称归档完成. 后续先同步并验证两份主 specs, 再执行归档/最终确认. 自然 loss、跨 GPU/OS、性能、致命 noexcept OOM、多周期计时以及非目标阶段仍维持已声明限制, 不新增验收门槛或扩大完成声明.

### 主规范同步与归档准备

2026-11-08: 按 `openspec-sync-specs` 的语义合并流程, 使用当前 status JSON 的两条 `artifactPaths.specs.existingOutputPaths` 和 nearest repo root, 同步全部且仅本 change 的 delta. 写入前成功取得一次 `instructions specs` 规则快照, 未配置额外 artifact rules; 不另造同步约束或绕过 CLI 验证.

- 新建 `opengl-completion-queries/spec.md`: Purpose 沿 delta 原文, 将 6 项 ADDED 要求和 18 个场景放入单一 Requirements 节, 不保留 delta 操作标题.
- 修改 `opengl-device-resources/spec.md`: 仅替换两项 MODIFIED 要求的描述/场景, 增加两个场景; 原 5+3 个场景标题全部保留. 主 Purpose 及另外 12 项要求原样保留, 不同步本 change 之外的能力.
- 两份 delta 的要求块与新主规范逐项一致; 全部 12 份主 specs 经 `openspec validate --specs --strict --json` 通过, 同步后 change strict 通过, `git diff --check` 通过. 同步摘要/前后身份在 `build/d03-t04/spec-sync.json`, 原资源主规范副本仅在本地 build 中保留.
- 当前 20/21: 唯一未勾选项为 8.3 的归档及归档后确认, 无未完成的实现、审查或验收项. 归档命令按已授权流程执行, 保持该项未完成直到实际归档、内容/链接核对及问题单模板重置成功; 不以提前勾选掩盖未发生的收尾.

归档源/目标均已解析为本仓库 `openspec/changes/` 与其 `archive/2026-11-08-establish-opengl-submission-retirement/`, 目标尚不存在. 两份主规范已同步并校验, 归档采用已同步模式避免二次写入; 不跳过 change validation. 当前问题单完整 B 签署/覆盖/限制已保存在本文件 b-final-review 等节, 原始问题单本地备份为 `build/d03-t04/CodeReview-before-archive.md`, 只有归档成功后才重置当前问题单.

<a id="archive-closeout"></a>
### 归档完成

2026-11-08: `openspec archive establish-opengl-submission-retirement --yes --skip-specs --json` 成功, 实际目标为 `openspec/changes/archive/2026-11-08-establish-opengl-submission-retirement/`. 主规范已在上一阶段同步并通过 12/12 strict 校验, 因而本次命令的 `specsUpdated=false` 仅表示不二次写入, 不表示漏同步; 未使用 no-validate. 命令执行时的 20/21 只剩归档后确认自身, 本节确认成功后才勾选 8.3.

实际核对了 proposal/design/tasks/两份 delta 与 `.openspec.yaml` 六份产物, 元数据与归档前相同, 活动目录已迁出; 两份 delta 的完整要求块与主规范一致, 其它主 specs 未变. 归档后修正了相对文档链接, 开发计划和 RHI 说明指向实际归档. B 最终签署、完整文件注释覆盖、CR 不适用原因、最终 verify、实际运行/复用证据及所有限制已保存在本 tasks, 当前问题单随后按 `Docs/CodeReview.md.tpl` 原字节重置.

最终任务为 21/21, 完整性/正确性/一致性检查及同步/归档收尾均完成, 无未解决 CR、功能阻止项、warning 或 suggestion. 本轮收尾只修改文档/规范与归档位置, 已验收的 15 文件代码摘要保持不变. D03-T04 工作包清单已完成, 下一包为 D03-T05; 稳定图像门槛、公共异步读回、Renderer/Editor、呈现与 D03 阶段出口未由本次归档提前完成. A 未暂存、提交或推送.
