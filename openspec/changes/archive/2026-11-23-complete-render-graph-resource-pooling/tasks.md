# Tasks

开发验证按 [构建入口](../../../../Docs/Build.md) 与 [最小验证约定](../../../../.agents/skills/physara-verification/SKILL.md) 执行。测试代码、样例、脚本和日志置于 `build/d08-t06/`。记录实际输入、命令、结果与覆盖边界。仅当输入、检查和执行环境匹配时复用证据。受控时序检查与真实后端证据分别标明，覆盖对应风险后停止扩展。

## 1. 导入事实和物理计划

- [x] 1.1 扩充 Texture 规范导入的内容覆盖，沿用 Buffer 区间及纹理交集机制，使声明、编译和执行读取同一份初始覆盖。用有效选层/选面和含空洞的读取或导出验证实际覆盖，核对等价重复导入会复用身份、冲突覆盖会拒绝。 [导入规范](specs/render-graph-contract/spec.md)
- [x] 1.2 复用完整 storage desc 兼容判定及逻辑字节估算，为存活槽增加对象数约束。以同纹理中仅一处被导出读取、独立分支无根、两项兼容资源间隔使用与同一 Pass 相交为一组输入，核对裁剪、图尾延寿和合并槽数。用刚好足够与少一个槽的约束检查 CPU 计划，确认没有执行或原生分配。 [计划规范](specs/render-graph-compilation/spec.md) · [物理计划设计](design.md#plan-and-pool)

本地 Debug 与 Release 的 `GraphPlan` 均通过。两种纹理覆盖拆分得到同一导入版本，空洞及未声明模板位被拒绝。编译仅保留三个 Pass，以三个槽计 1,984 B，两个槽时拒绝且回调未执行。[Debug](../../../../build/d08-t06/a/GraphPlan-Debug.log) · [Release](../../../../build/d08-t06/a/GraphPlan-Release.log)

## 2. 池容量、兼容获取和退休

- [x] 2.1 实现每设备会话共享的池与开录制前准入，按计划中的物理槽预留兼容存储或可新建额度。用较小的对象和字节限制核对命中从原容量转入本次计划、淘汰空闲后可准入、空池也不足时明确拒绝，失败或放弃不泄漏预留。 [容量设计](design.md#pool-budget) · [池规范](specs/render-graph-resource-pool/spec.md)
- [x] 2.2 从既有 Graphics 完成渠道推进在途项、最后一次使用、外部保留和空闲淘汰。以可控的 Pending、Complete 及观察失败验证只有真实完成与解除全部持有后才能归为可复用。验证对象数与逻辑字节的空闲保留上限，并分别辨认在途和外部持有造成的压力。 [退休合同](../../../../Docs/Architecture.md#retirement)
- [x] 2.3 将池接入执行器首用，命中沿真实末态接续，并据计划准备附件和绑定。以兼容 Texture、Buffer 和一个描述不相容的对照核对对象复用与创建点，原生内容预期来自本次写入。执行中止后检查尚未走到的槽没有物化，独立执行入口仍沿原合同工作。 [首次物化规范](specs/render-graph-execution/spec.md)

## 3. 跨图交接和原生失败路径

- [x] 3.1 在输出及导入交接只传递本录制能证明的状态和精确内容，登记所属设备与接受票据。原生回放让至少一个实际 Close 状态区别于图尾，并包含选定 mip/layer 的不同用途或使用不同录制的后继 Copy。核对跨列表导入用真实状态、有效范围的数据正确，越出覆盖或使用错误票据不能取得同一内容。 [状态交接设计](design.md#handoff-state)
- [x] 3.2 交付外部保留和最后使用的追踪，让显式的 UI/History ShaderRead 导入在同队列按接受顺序读取。用同次提交的同 View 颜色和深度作为一对，验证待完成仍可依队列消费、独立同描述候选不能取走受保留对象、后继消费者延长最后用时，新候选失败或与代不匹配不替换已确认对。保留记录的 GraphId 仅用于其本图。 [消费者设计](design.md#frame-owners)
- [x] 3.3 复用图内 CopySource/MarkReadback 及图外 Readback 出口接真实 staging。用独立已知选层像素或 Buffer 字节验证同批及另录制消费的合约，至少覆盖池源在其他对象继续获取时仍有效。复用匹配的 D08-T05 复制及查询用例，核对提前 Map 或取消关联时没有新结果，接受及 Event 真正完成后才可消费。 [读回规范](../../../specs/opengl-immediate-transfer/spec.md)
- [x] 3.4 对一次真实已发前缀制造后继首用或 recipe 失败，核对首错、未物化预留归还、没有成功出口及无后继 Pass。分辨可靠前缀票据和未获可靠票据或 context 无效时的受控出口，前者按实际完成证明退休，后者禁止继续借用。失效场景可控制验证，实际运行记录说明手段。 [失败协议](specs/render-graph-execution/spec.md)

## 4. Runtime、View 和 UI 消费

- [x] 4.1 正常图与单次独立安全 UI 均在 Open 前消费同一设备池准入结果。用主图无法纳入而安全图能纳入的样例核对旧输出或占位，继续收窄预算以核对安全图也失败时归还取得。记录首错、压力原因和有限重试。录制开始后的失败保持原前缀处理。
- [x] 4.2 把主 View、显式离屏 View、诊断色和 UI 实际输出接入跨帧持有。用尺寸或格式换代且旧图仍有采样的场景，核对稳定 token、两路各自身份、旧色保持及新色仅随接受发布。核对旧色在安全图再用时延寿，字体/占位既有生命周期仍能确认实际上传，窗口与 View 的尺寸继续独立。 [编辑器帧规范](specs/editor-frame-graph/spec.md)
- [x] 4.3 将图池释放接入设备会话的停帧和销毁顺序。借用、录制持有、实际设备依赖分别归原 owner。在一次伴随旧代或读回在途的关闭中，结合 3.4 的失效用例核对停止新提交和可用 context 下的正常退休。无需重复无关查询或传输矩阵。 [退出协议](../../../../Docs/Architecture.md#shutdown)

## 5. 诊断和 A 本地集成验收

- [x] 5.1 由计划、池和 Runtime 发布有界只读摘要及统计。核对声明/存活/裁剪、槽数与字节、命中/创建/淘汰、当前与峰值容量的口径。检查池压力及安全 UI 不覆盖最近接受场景的身份，面板显隐不影响计数、命令与非阻塞回收。 [统计规范](specs/render-frame-statistics/spec.md)
- [x] 5.2 在 Windows/OpenGL 实际运行正常编辑器图，用主/离屏目标、静止后重复尺寸和格式切换、全部裁剪的分支及小容量压力跨多个帧槽轮转。记录设备、驱动、配置、尺寸、格式、轮转、正常与回退票据、池峰值和已知像素对照，核对暖机重用、未使用资源零物化、陈旧空闲对象有界和真实完成前不复用。受控 Pending 的结果和原生回放各自记录。 [初始预算与代表输入](design.md#pool-budget)
- [x] 5.3 以当前正式输入构建直接受影响模块与 Physara 产品，完成 Debug/Release 受影响入口检查及 OpenSpec strict、链接与 diff 核对。记录执行命令、产物及关联输入身份、实际运行结果、复用与未覆盖范围。已经由 1 至 5.2 证明且输入相同的检查可直接引用。

<a id="a-local-verification"></a>
### A 本地验证记录

- Debug 与 Release 的 `PoolNative` 均通过。分别用真实 Graphics 包验证在途压力、外部保留压力、兼容 Texture/Buffer 复用、格式不兼容的淘汰、精确 mip/layer 和字节读回、非均匀出口的状态继承，以及图成功交接后 Close 恢复 Common。成对颜色和深度核对匹配票据，并在等待生产提交完成之前依次用新图导入。受控失败覆盖实际已发前缀、未获票据、未物化预留归还及 `GL_WAIT_FAILED` 后的有序收拢。12 次尺寸和格式切换的受控池峰值为 3 个对象、3,104 B，淘汰 10 项。 [Debug](../../../../build/d08-t06/a/PoolNative-Debug.log) · [Release](../../../../build/d08-t06/a/PoolNative-Release.log)
- Debug 与 Release 的 `RuntimePool` 链接当前 `Frame.obj` 和 `FrameStatistics.obj`。参考场景在 BaseColor 下同时渲染 1920×1080 和 1280×720，代表性存活计划为 6 槽、47,923,200 B，测试池峰值为 11 个对象、72,146,944 B，六帧覆盖三个槽并记录 18 次命中，首帧与复用帧像素相同。96 MiB 测试预算拒绝需要 119,513,600 B 的候选。独立安全 UI 接受票据 8 时保留场景票据 7 和旧色。零容量下主图与安全图均未打开录制，取得可再次使用。检查了 resize 的稳定 token、两路独立输出、主 Image 裁剪，以及统计值与可见面板文本。 [Debug](../../../../build/d08-t06/a/RuntimePool-Debug.log) · [Release](../../../../build/d08-t06/a/RuntimePool-Release.log)
- 当前源码的产品 Debug 和 Release 均构建成功，隐藏启动 `--hidden --poll-limit=12` 返回 0。沿产品资源表检查两种配置合计 20 个 Shader 包及 ABI。既有 D08-T05 图读回三种输入及 Runtime 计时回放用本次 Release 库与对应对象重新链接，均通过。OpenSpec 1.13.2 严格检查通过 1 个 change 和 38 份主规范，规划链接和 Git diff 已核对。 [构建 Debug](../../../../build/d08-t06/a/build-Debug-2.log) · [构建 Release](../../../../build/d08-t06/a/build-Release-2.log) · [既有图读回](../../../../build/d08-t06/a/GraphReadbackBaseline-Release.log) · [既有计时](../../../../build/d08-t06/a/RuntimeTimingBaseline-Release.log) · [OpenSpec](../../../../build/d08-t06/a/openspec-strict.json)
- 原生结果限于本机 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。失败的 fence 观察与拒绝提交通过受控条件触发。1080p/720p 结果用于资源计量和像素对照，GPU 性能、实际设备丢失、跨驱动和后续时域渲染生产留在各自的验证范围。 [正式输入、程序、包与证据清单](../../../../build/d08-t06/a/handoff.json)

<a id="independent-review"></a>
## 6. 独立 Review/Annotation 闭环

- [x] 6.1 Agent-A 在本地检查通过后填写当前问题单的实现交接。链接基准 commit、正式 diff 和新增文件，登记相关库、程序、检查输入与日志，说明结果与限制。交接时 B 的任务仍待检查。 [当前问题单](#review-closeout)
- [x] 6.2 Agent-B 独立按实际变化核查规范、代码和直接消费者，审查池所有权、完成票据和范围/状态合同。记录稳定 CR 编号和证据，按完整第一方涉及文件及直接消费者补齐职责、合同、流程说明并核对注释。 [首轮审查与注释](#review-closeout) · [独立审查技能](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 6.3 Agent-A 逐条按 CR 修正并运行有区分力的最小检查，在问题单登记变更、证据及输入身份。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2) · [第三轮整改](#a-correction-3) · [第四轮整改](#a-correction-4) · [第五轮整改](#a-correction-5)
- [x] 6.4 Agent-B 复核最终版本的行为、受影响合同和注释，已独立确认源码及所引用的运行证据匹配。CR-001 至 CR-012 全部关闭，B 已签收。 [首次整改复核](#b-recheck-1) · [第二轮整改复核](#b-recheck-2) · [第三轮整改复核](#b-recheck-3) · [第四轮整改复核](#b-recheck-4) · [第五轮复核与签收](#b-recheck-5)

### B 首轮审查与注释

2026-11-22，B 核对了 A 交接的全部 40 项正式输入，在 33 项补充或修正注释。Release 规划回放通过，资源池、UI 别名和双 View 回放以及 Debug 分配故障检查复现 CR-001 至 CR-007。两项 P1 涉及被覆写的旧读回内容和托管纹理别名。其余五项涉及 Debug 观察分配、未用预留、录制与外部持有的容量诊断，以及离屏图种类。代码验收待整改，问题单中附 B 注释后指纹、复现值与逐项复核条件。 [当前问题单](#review-closeout) · [两版源码与 B 证据](../../../../build/d08-t06/b-review/review-inputs.json)

<a id="a-correction-1"></a>
### A 首轮整改与自检

- CR-001 和 CR-002 的 Debug 与 Release 回放验证了已保留红色读回、独立子资源更新、失效覆盖及重写。托管 token 和驻留别名均得到 `v1`，两份实际读回取得相同的更新像素，别名的冲突事实和越权写入被拒绝。 [Debug](../../../../build/d08-t06/a-correction-1/PoolCorrections-Debug.log) · [Release](../../../../build/d08-t06/a-correction-1/PoolCorrections-Release.log)
- CR-003 重放 B 的 MSVC Debug 代理分配注入，观察返回空并留下分配首错。重试观察、接受前缀和实际完成后读回 42 均通过。 [Debug 故障记录](../../../../build/d08-t06/a-correction-1/ObserveOom-Debug.log)
- CR-004 至 CR-006 的 Debug 与 Release 回放验证了执行退出时归还第 3 槽，待提交、在途及完成统计的转换，以及外部 FBO 与仅随包保活的依赖。CR-007 的 Release 回放确认旧主视口和新离屏输出所在图仍报告正常，纯安全 UI 对照保留旧场景票据。 [容量 Release](../../../../build/d08-t06/a-correction-1/PoolCorrections-Release.log) · [双 View](../../../../build/d08-t06/a-correction-1/RuntimeReview-Release.log) · [安全 UI](../../../../build/d08-t06/a-correction-1/RuntimePool-Release.log)
- 当前正式输入对应 Debug/Release 产品构建和隐藏启动、20 个产品 Shader 包及 ABI、7 次重连回放。复用了上一轮规划、原生池和 Runtime 用例的输入与检查逻辑。原生环境为本机 Windows x64、Intel RaptorLake-S Mobile 和 OpenGL 4.6.0 Build 32.0.101.6129。分配与完成观察失败由受控注入触发。 [源码、程序与证据清单](../../../../build/d08-t06/a-correction-1/handoff.json) · [逐项整改](#review-closeout)

<a id="b-recheck-1"></a>
### B 首次整改复核

2026-11-22，已核对 A 整改版 `8a6b4bb613304ece7621bb241abb97500262f7e0ce97c8b8e3a9f7043f834da9` 的 43 项正式输入，较 B 首轮注释版有 15 项改动。B 独立重跑了 Release 规划、资源池整改、原生池、双 View、Runtime 安全 UI 和 Debug 观察分配回放。旧读回保持红色，Debug 前缀读回 42，槽归还、容量分类、外部 FBO 和混合图统计均通过。CR-001、003、004、005、006、007 复核关闭。 [资源池](../../../../build/d08-t06/b-recheck-1-independent/PoolCorrections-Release.log) · [Debug 前缀](../../../../build/d08-t06/b-recheck-1-independent/ObserveOom-Debug.log) · [双 View](../../../../build/d08-t06/b-recheck-1-independent/RuntimeReview-Release.log) · [安全 UI](../../../../build/d08-t06/b-recheck-1-independent/RuntimePool-Release.log)

CR-002 保持开放。同图中只读别名可以转授写权限。新增 CR-008，普通用途准备失败且写回调尚未进入时，64 个原始红色像素仍在，下一图却拒绝 Load。新增 CR-009，托管 token 销毁后，同对象的两个有效驻留别名在纯读取图中发生导入冲突。三项交回 A，6.4 待验收。B 在 9 个文件完善注释并核对仅有注释差异，交回版本指纹为 `d134914d34809d9232c72c378b09e7900c71e238c567f050a10f4c80722d66ce`。 [问题单与逐项条件](#review-closeout) · [补充回放](../../../../build/d08-t06/b-recheck-1-independent/CorrectionEdges.log) · [两版源码、产物与证据清单](../../../../build/d08-t06/b-recheck-1-independent/review-inputs.json)

<a id="a-correction-2"></a>
### A 第二轮整改与自检

- CR-002 与 CR-009 在 Debug 和 Release 上通过。手持同一 `v1` 的读者无法写入、转授写权限或用重建引用取得写入对象。托管 token 销毁后，两个驻留别名以两种顺序解析，均共享同一 `v0` 和实际纹理。冲突范围和状态仍拒绝。 [Debug](../../../../build/d08-t06/a-correction-2/CorrectionEdges-Debug.log) · [Release](../../../../build/d08-t06/a-correction-2/CorrectionEdges-Release.log)
- CR-008 在 Debug 和 Release 上通过。受控普通用途准备失败发生在目标写入之前，前缀获票据 7，回调为 0，64 个原始红色像素仍有效且 Load 随后成功。真实复制发出后失败的范围需重新定义。一个 Clear 已发出时，未进入的普通写目标继续保留内容。 [同批回放](../../../../build/d08-t06/a-correction-2/CorrectionEdges-Release.log)
- 本轮核对并采用工作区中尚未登记的第二轮代码。当前 45 项正式输入涵盖 Forward 与出口状态规划对图身份访问器的迁移。核对 19 项 B 登记证据，重新验证 Debug/Release 产品构建、隐藏启动和 20 个 Shader 包。6 次新链接回放覆盖上述问题、CPU 规划、旧读回及预算行为、混合离屏场景和安全 UI。源码与 74 项构建输入和产物在最终增量检查前后散列一致。原生环境为本机 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。失败准备通过受控分配故障触发。 [逐项结论](#review-closeout) · [源码、构建与回放身份](../../../../build/d08-t06/a-correction-2/handoff.json)

<a id="b-recheck-2"></a>
### B 第二轮整改复核

2026-11-22，B 核实 A 整改版 `5ab57c37866502009a4e68d67e79f8e28681982a5217b7720dc428271428b4e2` 的 45 项正式输入，较上次 B 版有 14 项变化。六个登记程序独立复跑通过。CR-002 的采样别名、公开声明及重建引用均未取得写权限。CR-008 在写回调开始前失败时保留全部 64 个红色像素，可重试 Load，实发复制后的失败仍保守撤销目标覆盖。CR-009 在托管 token 销毁后两种解析顺序均可采样。三项本轮通过，CR-001 至 CR-009 均已关闭。 [Debug](../../../../build/d08-t06/b-recheck-2-independent/CorrectionEdges-Debug.log) · [Release](../../../../build/d08-t06/b-recheck-2-independent/CorrectionEdges-Release.log) · [资源池回归](../../../../build/d08-t06/b-recheck-2-independent/PoolCorrections-Release.log)

B 用当前头和库另行链接的 Release 回放发现两项顺序依赖。CR-010 中，先导入普通只读别名时，同图后导入的固定快照失去保护，另一图覆写全部 64 个红色像素，读图以 `StateMismatch` 退出。CR-011 中，同版本先导出只读快照时，UI 创建回执保存错误的出口，下一次区域更新返回 `InvalidResource`。两项交回 A，6.4 待验收。B 在 9 个文件完善注释并核对修改块，交回指纹为 `cf8f62d7f9a0762a1859ca80b35ba2a9146ecc8a3a0626767f4b5882a1bb71c5`。旧本地测试辅助源码 `build/d08-t03/final/Indirect.cpp` 缺失，独立链接时使用 B 的辅助代码，保留了本轮整改用例原文。 [问题单](#review-closeout) · [顺序与像素对照](../../../../build/d08-t06/b-recheck-2-independent/HandoffOrder.log) · [输入及产物清单](../../../../build/d08-t06/b-recheck-2-independent/review-inputs.json)

<a id="a-correction-3"></a>
### A 第三轮整改与自检

- CR-010 的 Debug 与 Release 对照均在交换导入顺序后拒绝相交写，读图保留全部 64 个红色像素。两份固定快照以相同规范范围导入时，两处固定保护均随图保留，未保护的子资源仍可写。图释放后恢复原地更新，普通别名继续跟随对象内容。 [Debug](../../../../build/d08-t06/a-correction-3/HandoffOrder-Debug.log) · [Release](../../../../build/d08-t06/a-correction-3/HandoffOrder-Release.log)
- CR-011 在两种出口顺序下均确认托管创建，实际完成下一帧的局部更新。更新像素为 `(27,139,217,255)`，其它 63 个保持红色。对外的只读出口不能取得写权限，只有只读出口时托管创建保持待处理。 [双顺序及缺失出口对照](../../../../build/d08-t06/a-correction-3/HandoffOrder-Release.log)
- 产品 Debug、Release 构建和隐藏启动通过。三次重新链接的原生回放覆盖两项整改、前一轮三项行为及编辑器双视口与安全 UI；20 个产品 Shader 包及 ABI 已核对。工具与临时回放位于 `build/d08-t06/a-correction-3/`，采用 B 留存的本地辅助源码。原生结果限本机 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。 [源码、构建及回放身份](../../../../build/d08-t06/a-correction-3/handoff.json) · [Runtime 回放](../../../../build/d08-t06/a-correction-3/RuntimePool-Release.log)

<a id="b-recheck-3"></a>
### B 第三轮整改复核

2026-11-22，B 核实 A 整改版 `f4b58ac17cdc4c9e6a86e7bff6b36ac9bca1c6f4fc449933c3e6ae779b6bfe7f` 的 45 项正式输入，以及相对上一 B 版的 6 项改动。Debug、Release 双顺序回放和 Release 编辑器帧回放均通过。CR-010 的两种导入顺序都保护了固定范围，保留多个固定范围及释放后的写入也符合预期。CR-011 的两种出口顺序都取得正确的托管更新权，目标像素更新且其余 63 个保持红色，只有只读出口时创建仍待处理。两项已由 B 勾选，CR-001 至 CR-011 全部关闭。 [Debug](../../../../build/d08-t06/b-recheck-3-independent/HandoffOrder-Debug.log) · [Release](../../../../build/d08-t06/b-recheck-3-independent/HandoffOrder-Release.log) · [编辑器帧](../../../../build/d08-t06/b-recheck-3-independent/RuntimePool-Release.log)

新增 CR-012（P2）。已编译读图固定 `mip 0/layer 0`，后续提交只写 `mip 1/layer 1`。固定区域的入口状态仍为 `Common`，64 个红色像素均保留，原读图却以 `StateMismatch` 退出，重新声明同一输入才可执行。工作包交回 A，仅此项开放，6.4 待验收。B 在 4 个文件补充注释，核对修改块后交回的正式输入指纹为 `891e66a40f8f21c23efd81522861694fb5fcb4ac3d4e70b7e3d201a42d16f764`。 [问题单及复核条件](#review-closeout) · [子资源对照](../../../../build/d08-t06/b-recheck-3-independent/DisjointReader.log) · [源码和产物清单](../../../../build/d08-t06/b-recheck-3-independent/review-inputs.json)

<a id="a-correction-4"></a>
### A 第四轮整改与自检

- CR-012 的执行预检在最新已接受观察改变时，按固定纹理导入的实际 mip/layer 核对入口状态。Debug 和 Release 回放中，原编译读图在不相交写入后交付 64/64 个红色像素，另一子资源为 16/16 个绿色像素。接受票据由 `2` 经写入推进至 `3`，读图提交后推进至 `4`。 [Debug](../../../../build/d08-t06/a-correction-4/DisjointReader-Debug.log) · [Release](../../../../build/d08-t06/a-correction-4/DisjointReader-Release.log)
- 后续写入已 Close、尚未被接受时，原计划以 `StateMismatch` 拒绝。该提交获接受后，保护外的状态由 `ShaderResource` 变为 `Common`，同一读图计划执行成功。若保护内的入口从 `Common` 变为 `ShaderResource`，持有仍可读，但按旧入口编译的图以 `StateMismatch` 拒绝。Release 复跑确认两种规范导入顺序的相交写仍返回 `AccessDenied`。 [状态对照](../../../../build/d08-t06/a-correction-4/DisjointReader-Release.log) · [相交写对照](../../../../build/d08-t06/a-correction-4/HandoffOrder-Release.log)
- Debug、Release 产品复建及隐藏启动通过。比对工具、ABI 声明与包的文件身份后复用上一轮 20 个 Shader 包检查。45 项正式输入中只有 `GraphExecutor.cpp` 较 B 注释版变化，交接指纹为 `b13e1798fc7fc6cc4a3367e6a9bc3fb6b898accd7b1aba730ee6715627b922a4`。本轮原生回放限 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。CR-012 和任务 6.4 待 B 独立复核。 [交接清单及命令](../../../../build/d08-t06/a-correction-4/handoff.json) · [问题单](#review-closeout)

<a id="b-recheck-4"></a>
### B 第四轮整改复核

2026-11-23，B 核实 A 整改版 `b13e1798fc7fc6cc4a3367e6a9bc3fb6b898accd7b1aba730ee6715627b922a4` 的 45 项正式输入，较上一 B 版仅 `GraphExecutor.cpp` 改变。Debug、Release 的显式固定范围对照和 Release 相交写回归均独立通过。上轮 B 原始用例重连本轮 Release 库后也通过。已编译读图交付 64/64 个红色像素，保护外为 16/16 个绿色像素。未接受录制及保护范围内的真实入口变化均返回 `StateMismatch`。 [Debug](../../../../build/d08-t06/b-recheck-4-independent/DisjointReader-Debug.log) · [Release](../../../../build/d08-t06/b-recheck-4-independent/DisjointReader-Release.log) · [原始 B 用例](../../../../build/d08-t06/b-recheck-4-independent/OriginalReader.log) · [相交写保护](../../../../build/d08-t06/b-recheck-4-independent/HandoffOrder-Release.log)

CR-012 部分通过，保留原编号。B 用 `View::LastAcceptedOutput()` 的同代只读显示色预先编译两张读回图。第一图获接受后，入口仍为 `ShaderResource`，第二图却在预检返回 `StateMismatch`，复制回调为零。重新声明的读图成功，与第一图读回的全部 64 个像素一致。普通 View 快照没有原地更新权，其存储也没有可更新纹理的显式范围登记。此路径交回 A，6.4 仍待验收。B 补充并核对执行器注释，交回指纹为 `857b5e71f40f342c47699eb342ec81cdfc0d4686d0db178f53bea51e13a9251b`。 [当前问题单](#review-closeout) · [View 双读图回放](../../../../build/d08-t06/b-recheck-4-independent/ViewReaders.log) · [源码及产物清单](../../../../build/d08-t06/b-recheck-4-independent/review-inputs.json)

<a id="a-correction-5"></a>
### A 第五轮整改与自检

- CR-012 的只读池项通过持有固定内容，执行预检在最新已接受观察推进后逐个核对声明范围的 mip/layer 状态。Debug 和 Release 重新链接 B 原有 View 回放。两张预编译读回图各复制一次，同代显示色的 64/64 个像素一致。第一图获接受后，最近使用票据从 `1` 推进到 `2`，入口保持 `ShaderResource`。 [View Debug](../../../../build/d08-t06/a-correction-5/ViewReaders-Debug.log) · [View Release](../../../../build/d08-t06/a-correction-5/ViewReaders-Release.log)
- 可原地更新的存储仍依赖覆盖声明范围的固定保护。Owner 覆写纹理后，跟随别名的入口仍为 `Common`，旧计划返回 `StateMismatch`，重新声明读到 64/64 个绿色像素。Debug 和 Release 子资源回放复核不相交写入后继续读取、未接受录制和保护范围内实际入口变化。Release 复跑确认两种规范导入顺序下的相交写仍返回 `AccessDenied`。 [子资源 Debug](../../../../build/d08-t06/a-correction-5/DisjointReader-Debug.log) · [子资源 Release](../../../../build/d08-t06/a-correction-5/DisjointReader-Release.log) · [相交写对照](../../../../build/d08-t06/a-correction-5/HandoffOrder-Release.log)
- Debug、Release 产品构建及隐藏启动通过。比对工具、ABI 声明与包的文件身份后复用 20 个 Shader 包检查。45 项正式输入中仅 `GraphExecutor.cpp` 较 B 注释版变化，交接指纹为 `07d019f1347437f790b984dcedad624570eb85ba5050eac3cda42b64217c4bbd`。本轮原生回放限 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。CR-012 和任务 6.4 待 B 独立复核。 [交接清单及命令](../../../../build/d08-t06/a-correction-5/handoff.json) · [问题单](#review-closeout)

<a id="b-recheck-5"></a>
### B 第五轮复核与签收

2026-11-23，B 完成独立验收。分支 `v2.0`，基准 `6a1db24f2d33f2ae63c9057142c7486c70539b3e`。45 项正式输入含 3 项未跟踪池源码。A 第五轮指纹为 `07d019f1347437f790b984dcedad624570eb85ba5050eac3cda42b64217c4bbd`。B 在 `GraphExecutor.cpp` 完善两行注释，核对整份输入的非注释代码、行数和宏续行，最终签收指纹为 `54791a9d83ae40da7a227f8b4f14850cab52d35e7ae9d211a8db21e07d5f29ba`。 [两版逐文件身份、程序及日志](../../../../build/d08-t06/b-recheck-5-independent/review-inputs.json)

- **CR-012 通过。** B 的原始只读 View 回放在 Debug、Release 均得到两次实际复制，64/64 个像素相同。最近使用票据从 1 推进至 2 时，入口保持 `ShaderResource`，第二张预编译读图直接成功。显式保护区域在不相交写入后仍交付原有的 64 个红色像素。可更新纹理被覆写后，即使入口仍为 `Common`，跟随别名的旧计划仍以 `StateMismatch` 拒绝，重新声明读到 64 个绿色像素。未接受录制、保护内入口变化以及相交写的拒绝均已复核。 [View Debug](../../../../build/d08-t06/b-recheck-5-independent/ViewReaders-Debug.log) · [View Release](../../../../build/d08-t06/b-recheck-5-independent/ViewReaders-Release.log) · [子资源 Debug](../../../../build/d08-t06/b-recheck-5-independent/DisjointReader-Debug.log) · [子资源 Release](../../../../build/d08-t06/b-recheck-5-independent/DisjointReader-Release.log) · [相交写与出口顺序](../../../../build/d08-t06/b-recheck-5-independent/HandoffOrder-Release.log)
- **闭环与注释。** 既有 CR-001 至 CR-007 的读回、别名、失败前缀、容量及场景统计，CR-008、009 的内容覆盖与 token 寿命，CR-010、011 的快照保护与托管出口均已复核关闭。本轮确认了只读池项的内容持有、可更新池项的范围保护，以及逐 mip/layer 入口与首用登记的说明。签收版执行器与原始 View 用例另行链接 Release 程序并通过。 [首次复核](#b-recheck-1) · [第二轮复核](#b-recheck-2) · [第三轮复核](#b-recheck-3) · [签收源码读回](../../../../build/d08-t06/b-recheck-5-independent/SignedViewReaders-Release.log)
- **验证边界与交接。** 原生结果限本机 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。B 核对当前清单中的 165 个独立文件，独立运行 5 个登记程序及 1 个签收源码回放。完整产品构建、隐藏启动及 20 个 Shader 包的 ABI 检查沿用 A 的匹配记录。代码正确性和注释可读性均已签收，开放的正式问题为零。第 7 节由 A 按签收版本完成。 [A 第五轮交接](#a-correction-5) · [当前问题单](#review-closeout)

<a id="final-verification"></a>
## 7. 最终 verify、同步和归档

- [x] 7.1 Agent-A 在 B 签收版本执行最终 verify。核对 D08-T06 的完成条件、实现对应关系及技能规定的验证范围，留存 CR、注释覆盖、最终版本、真实运行与边界。 [签收版验证](#signed-verification) · [审查留存](#review-closeout)
- [x] 7.2 同步六份增量规范到主规范，核对改动要求、原有场景和未涉及条目，完成主规范严格校验。 [主规范同步](#spec-sync)
- [x] 7.3 用项目锁定的 OpenSpec 流程归档，核对归档内容、链接和开发计划完成状态，留存签收证据并复原当前问题单。 [归档收尾](#archive-closeout) · [交付流程](../../../../AGENTS.md#review-loop)

<a id="signed-verification"></a>
### 签收版验证

工作区基准 `6a1db24f2d33f2ae63c9057142c7486c70539b3e` 的 45 项正式输入与 B 签收指纹 `54791a9d83ae40da7a227f8b4f14850cab52d35e7ae9d211a8db21e07d5f29ba` 一致，包含 3 项新增池源码。构建前核对 165 项 A 登记身份及 19 项 B 登记身份，确认 A 实测版和 B 签收版只有 `GraphExecutor.cpp` 的两行注释差异。逐行核对非注释代码、行数和宏续行后复用既有独立复核。签收版重新构建 Debug 和 Release，隐藏启动均返回 0。20 个 Shader 包、ABI 声明和检查工具的文件身份匹配，复用既有包检查。OpenSpec 1.13.2 严格校验本 change 通过。 [签收源码、差异与复用证据](../../../../build/d08-t06/final/signed-source.json) · [构建和运行清单](../../../../build/d08-t06/final/verification-evidence.json) · [变更严格校验](../../../../build/d08-t06/final/validate-change.json)

| 验收范围 | 实现与证据 |
| --- | --- |
| 图导入与物理计划 | `RenderGraph::ImportRetained`、`GraphCompiler` 和 `GraphPlanStorage` 保留精确范围、内容覆盖和存活槽寿命。Release 规划回放通过拆分范围等价、空洞拒绝、mip 根裁剪、相交寿命、图尾寿命及字节和对象约束。 [规划回放](../../../../build/d08-t06/final/GraphPlan-Release.log) |
| 准入、首用、回收与 resize | `GraphResourcePool::Admit`、`Collect` 和 `GraphExecutor::Acquire` 对兼容存储逐槽保留与复用。Release 资源池回放通过容量压力、撤销预留、命中及不兼容淘汰。12 次交替尺寸和格式的受控结果为峰值 3 个对象、3,104 逻辑字节，淘汰 10 项。 [图池回放](../../../../build/d08-t06/final/PoolNative-Release.log) |
| 交接、原生读回与失败 | `GraphExecutor` 接续实际 Close 状态，`GraphPoolEntry` 按实际接受及完成记录后继访问。回放通过 Texture 选层和 Buffer 字节读回、匹配色深、Event 完成前后的访问、两张预编译 View 读图，以及可接受前缀、拒绝提交前缀和注入 `GL_WAIT_FAILED` 后的收拢。拒绝提交的旧夹具计数已对齐首轮整改采用的 `PoolRecording` 合同。 [资源池及失败回放](../../../../build/d08-t06/final/PoolNative-Release.log) · [View Debug](../../../../build/d08-t06/final/ViewReaders-Debug.log) · [View Release](../../../../build/d08-t06/final/ViewReaders-Release.log) |
| Runtime、UI 与统计 | `Runtime/Frame.cpp` 在录制前准入，`View` 和 UI 对提交与输出代确认，`Workspace` 从只读快照显示。Release 双 View 为 1920×1080 和 1280×720，代表计划 6 槽、47,923,200 逻辑字节，受控帧池峰值 11 个对象、72,146,944 逻辑字节，18 次命中。96 MiB 注入预算下安全 UI 票据 8 保留场景票据 7，零容量对照中两图均在录制前拒绝。 [编辑器回放](../../../../build/d08-t06/final/RuntimePool-Release.log) |

复核 proposal、design 的五项决定、六份增量规范和直接生产者及消费者，确认池、图、录制包和输出 owner 各自维持既定职责。原生回放范围为 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。失败完成观察由受控注入触发。跨设备驱动、真实设备丢失、性能及后续时域算法未纳入原生验证。

<a id="review-closeout"></a>
### 审查闭环留存

B 已独立确认代码正确性和注释可读性。覆盖图声明、编译与执行、跨帧资源池、RHI 录制和退休、Forward、View、编辑器 UI 与 Runtime 的文件职责、公开合同和关键流程。签收问题单原件保留全部 A/B 勾选与逐项证据。 [B 最终验收](#b-recheck-5) · [签收问题单原件](../../../../build/d08-t06/final/review-signed.md)

| 编号 | 签收结论 |
| --- | --- |
| CR-001 | 固定读回出口保持所声明的旧内容，相交更新由保护范围拦截。 |
| CR-002 | 等价托管纹理别名解析同一版本，只读句柄无法转授写权限。 |
| CR-003 | Debug 的观察分配失败保留首错和实际已发前缀，完成后可继续使用。 |
| CR-004 | 执行失败后立即归还未首用槽，已实际访问的槽依提交条件退休。 |
| CR-005 | 尚未接受的录制计为录制或预留，接受后才计入在途。 |
| CR-006 | 完成后仍被附件持有的纹理按保留压力诊断。 |
| CR-007 | 正常离屏提交的图摘要与安全 UI 候选各自保留真实身份。 |
| CR-008 | 写回调尚未进入时保留旧内容，已实发且结果无法证明的范围撤销有效性。 |
| CR-009 | 托管 token 销毁后，仍持有池租用的只读别名解析同一对象。 |
| CR-010 | 两种等价导入顺序均保留固定范围并拒绝相交写。 |
| CR-011 | 两种等价出口顺序均选择具备更新权的 UI 创建回执。 |
| CR-012 | 只读 View 及范围受保护的快照可按真实状态接续已接受观察，继续拒绝失配入口和可更新存储的过期跟随读图。 |

<a id="spec-sync"></a>
### 主规范同步

六份增量规范中，`render-graph-resource-pool` 创建主规范，`render-frame-statistics` 新增一项要求，`editor-frame-graph`、`render-graph-compilation`、`render-graph-contract`、`render-graph-execution` 合入七项修订。逐条核对同步的是上述四项新增及七项修订，保留全部原有场景及 29 项未涉及要求，原有主规范的目的和顺序保持。OpenSpec 1.13.2 严格校验 39 份主规范全部通过。 [同步逐项凭据](../../../../build/d08-t06/final/spec-sync.json) · [全量主规范校验](../../../../build/d08-t06/final/validate-specs.json)

<a id="archive-closeout"></a>
### 归档收尾

OpenSpec 1.13.2 在主规范同步后归档本 change，22 项任务全部完成。归档后的活跃变更列表为空，39 份主规范再次通过严格校验。九份归档工件和开发计划的引用已核对，六份原增量规范逐字节保留。D08-T06 已验收并归档，开发计划指向 D08-T07。B 的签收问题单原件已留存，当前问题单已按模板复原。 [归档回执](../../../../build/d08-t06/final/archive.json) · [归档后校验](../../../../build/d08-t06/final/post-archive-validation.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [B 签收原件](../../../../build/d08-t06/final/review-signed.md)
