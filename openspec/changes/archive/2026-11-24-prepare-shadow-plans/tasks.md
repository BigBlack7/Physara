# Tasks

## 1. 请求与保守阴影视图

- [x] 1.1 A 接入按 View 身份提交的会话阴影请求，解析设备的 D32 分层能力与有效规划上限。用 V2 核对设备尺寸导致的质量回退，用 V4 的非法距离核对 Release 请求拒绝、原请求和现有相机准备。 [View 规格](specs/view-preparation/spec.md) · [请求设计](design.md#ownership-and-requests)
- [x] 1.2 A 从稳定有限相机构造方向光接收段，用冻结 Primitive 表拟合深度和构建保守 caster 集。用 V1 的屏内接收物、画外遮挡物、禁用投射物与分离物核对范围和索引。 [保守阴影视图](design.md#conservative-shadow-views)
- [x] 1.3 A 在同一计划中交付聚光外锥与有限远面、点光六面与统一近远面、各组保守候选和常规深度约定。用 V1 核对无限光照与有限阴影、点光面身份及局部范围的保守筛选。 [阴影规划规格](specs/shadow-planning/spec.md)

## 2. 分配与静态缓存

- [x] 2.1 A 实现优先级、质量档、字节及层准入，以稳定灯身份发布子视图和区域代。用 V2 核对整组六面、层跨度、兼容区域稳定、删除及地址复用，并确认被拒绝阴影的灯继续在完整灯表中。 [分配设计](design.md#allocation-and-light-indices)
- [x] 2.2 A 按 View、Scene 与实际投影和 caster 依赖缓存完整计划，维护区域的所需内容版本。用 V3 核对跨槽复用、相关编辑及失效，使用同一序列确认旧槽只读数据仍对应原准备。 [缓存设计](design.md#plan-cache-and-invalidation)

## 3. 正常帧与只读诊断

- [x] 3.1 A 在共享 SceneFrame 的正常主 View 和离屏 Prepare 交付计划。用 V4 核对两路独立请求、无效布局、恢复及禁用，确认完成的计划可从当前灯解析到区域和 caster。 [View 规格](specs/view-preparation/spec.md)
- [x] 3.2 A 接通 Renderer 面板的 CPU 规划开关和既有统计的只读摘要。用 V4 比对本次计划身份及逻辑额度、上一接受票据和面板显隐，确认光照和提交继续有效。 [统计规格](specs/render-frame-statistics/spec.md) · [诊断设计](design.md#plan-diagnostics)

## 4. A 集成自检与交接

- [x] 4.1 A 构建正式受影响消费者，汇总 V1–V4 的本地证据和 OpenSpec 严格校验。登记基准提交、实际 diff、未跟踪正式输入、可复现检查、文件身份和限制，在当前问题单交维护者调度独立 B。 [本地证据](#a-local-verification) · [当前问题单](../../../../build/d09-t01/final/review-signed.md)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核查版本、保守几何、成组分配、失效与正式帧消费者，复核代表结果。按项目约定检查并完善本轮涉及的完整第一方文件的职责、接口和流程注释，在现行问题单记录结论和发现。首轮重跑 V1–V4 通过，另复现 CR-001 至 CR-006，待 A 整改。 [当前问题单](../../../../build/d09-t01/final/review-signed.md) · [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 5.2 A 按正式 CR 编号整改并自检受影响场景，记录当前版本、匹配检查和限制后交回复核。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2)
- [x] 5.3 B 复核最终整改、匹配证据和注释，记录关闭状态并签收实际版本。首轮整改复核关闭 CR-003 至 CR-006，第二轮关闭 CR-001、CR-002，签收版本及运行证据已留存。 [首轮复核](#b-recheck-1) · [最终签收](#b-recheck-2)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后核对源码、正式构建与输入身份，对签收版本完成最终 verify。确认 V1–V4 和 D09-T01 完成条件，采用匹配的 CPU 检查及正式 OpenGL 正常帧验证，记录实跑、复用和适用范围。 [最终验证](#final-verification)
- [x] 6.2 A 在 change 留存 B 签收、正式问题与验收证据。同步三份增量，核对新 `shadow-planning` 及两份扩充的主规范，运行严格校验。 [规范同步](#spec-sync)
- [x] 6.3 A 执行 OpenSpec 归档，复核归档位置、链接与任务状态。更新 D09-T01 状态及 D09-T02 前置，在签收证据留存后按模板复位问题单。

<a id="verification-scope"></a>
## 验证口径与停止条件

| 检查 | 变化风险 | 最小可判定结果 |
|---|---|---|
| V1 光空间候选 | 沿用主可见集，或把局部阴影距离用于截断光照 | 一份小场景确定光线会穿过主相机画外 caster 到达接收点。用分离和禁用投射的记录对照。以有限正交和无限远透视复用这组几何，核对稳定有限范围、深度约定和实际 jitter 下的索引。局部灯用一盏显式 range 的聚光及一盏无 range 的点光验证有限阴影和原灯语义。 |
| V2 容量及地址 | 点光部分发布、字节少算、复用旧代或失败后丢光 | 1024 档的一层 D32 为 4 MiB。给一盏方向光、一盏聚光和一盏点光八层上限，先给 28 MiB 再给 32 MiB，点光应从整组拒绝变为六面同时准入。删掉并替换聚光，核对兼容层保留、跨度仍含空层及新区域代。收紧可用边长确认有效质量换代和上限，核对完整灯输入。 |
| V3 实际依赖 | 槽轮转造成重复拟合，或移动／移除后复用旧阴影 | 暖机后以一个冻结序列跨槽核对缓存命中、零区域版本变化及分配稳定。依次变更曝光和实际 jitter、把画外 caster 移出覆盖、改一个有效 MASK 覆盖输入，核对各步期望的区域版本与投射物范围，并核对旧槽数据及另一 View 的独立输入。 |
| V4 正式准备与状态 | 请求错误、切换或失败时显示陈旧区域，CPU 计划被当作已接受输出 | 用正常主 View 和离屏入口准备同一 Scene 的两个有效请求，切换其中一路。核对非法距离、禁用、尺寸无效及改变投影后恢复，结合一次正常场景未获接受的候选确认当前规划摘要和旧票据。保持同一输入对照面板显隐，从实际输出及统计核对直接光继续参与。 |

先使用受影响的既有检查，再为各组仍缺的可观察结果补最小探针。复用同一场景和回放覆盖相邻任务，开发验证代码、样例和原始结果放在 `build/d09-t01/`。原生检查限于正式产品和真实 OpenGL 现场中的直接光、View 准备与统计发布，后续工作包负责深度纹理、过滤可见性与性能测量。达到四组结果且证据与验收输入匹配后停止。 [构建与运行](../../../../Docs/Build.md) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## A 本地验证

- V1–V3 用同一组内存静态几何，经正式 Scene 事务、SceneFrame 和 ViewWork 检查 Release 结果。有限正交与无限远渲染透视均保留同一画外 caster，方向光光空间深度覆盖到 −4.25 m。点光申请在 28 MiB 时整组拒绝，在 32 MiB 时获得六面，删灯后 7 个区域仍计 8 层跨度。复用层 1 时区域代由 1 推进至 4，设备边长受限时降到 512。三槽暖机后复用同一不可变计划，曝光、实际 jitter 和灯光强度变化保持所需内容版本，移出画外 caster 后 7 个区域变化，MASK 覆盖编辑使受影响的 8 个区域重新标记。 [CPU 输入](../../../../build/d09-t01/a/ShadowFixture.hpp) · [CPU 检查](../../../../build/d09-t01/a/ShadowCpu.cpp) · [实跑输出](../../../../build/d09-t01/a/cpu-release.log)
- V4 在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 上使用真实 DeviceHost、正常 Runtime 帧及 Editor。设备 D32 分层能力读出 16384 texel、2048 层。两路同场景计划分别为 32 MiB 和 8 MiB 的逻辑跨度。预算拒绝正常图时，已更新的主 View CPU 计划与上次场景票据 5 同时可读。离屏无效尺寸释放 8 个区域，恢复后发布当前相机的计划。面板显隐和关闭主 View 规划前后，640×480 接受场景的颜色字节一致，仍有 4 盏直接光。面板显隐通过临时 Workspace 副本调用既有入口，正式帧、设备与其他消费者使用当前正式构建。 [原生检查](../../../../build/d09-t01/a/ShadowNative.cpp) · [实跑输出](../../../../build/d09-t01/a/native-release.log)
- Windows x64 正式 Debug、Release 均构建通过，两份 `Physara.exe --hidden --poll-limit=15` 均退出 0。两个检查程序以 `BuildProbe.ps1` 和构建的库链接。D09-T02–T05 各自验收实际阴影生产、采样、设备压力和画质。 [构建 Debug](../../../../build/d09-t01/a/build-debug.log) · [构建 Release](../../../../build/d09-t01/a/build-release-2.log) · [检查程序构建](../../../../build/d09-t01/a/BuildProbe.ps1)
- OpenSpec 1.13.2 对 change 与 40 份现行主规范执行严格校验。基准提交、差异、正式文件、合成输入、Shader 包、库、二进制及运行命令由 A 交接清单登记。 [OpenSpec 校验](../../../../build/d09-t01/a/openspec-validation.json) · [A 交接清单](../../../../build/d09-t01/a/handoff.json)

<a id="a-correction-1"></a>
## A 首轮整改

- 核对 B 注释后的 1202 个正式输入，源码指纹与问题单一致。使用 B 登记的程序实跑并复现六项，再用原 B 探针源码和修正版 Release 库及 Runtime 对象重链。 [B 版 CPU 重放](../../../../build/d09-t01/a-correction-1/before-cpu.log) · [B 版原生重放](../../../../build/d09-t01/a-correction-1/before-offscreen.log) · [整改差异](../../../../build/d09-t01/a-correction-1/correction-from-b.patch)
- CR-001、CR-002 按有限灯体和稳定接收凸体的分离结果准入，保留相切。相同 7 层预算下，背向聚光释放单层给点光六面，朝内仍保留聚光优先级。正交及透视棱角各有分离和相切对照。CR-003 逐面保留旧层，5 面保持原分配代及内容版本，迁移 1 面，释放 2 个旧区域。CR-005 在 D32 数组不可用时记录 4 盏灯的整体拒绝。CR-006 核对设备限制与字节预算进一步降档的两个明确原因。 [原 CPU 探针](../../../../build/d09-t01/a-correction-1/Boundary.log) · [视锥与原因对照](../../../../build/d09-t01/a-correction-1/ReceiverCases.log)
- CR-004 在 Intel RaptorLake-S Mobile 的正式 Runtime 帧回放中，真正属于离屏 View 的陈旧布局返回拒绝摘要和旧层释放数，主 View 正常接受。恢复后发布新区域代。不同归属的布局和重复主 View 保留各活动计划。 [陈旧布局](../../../../build/d09-t01/a-correction-1/Offscreen.log) · [身份隔离](../../../../build/d09-t01/a-correction-1/OffscreenIsolation.log)
- 重建 Windows x64 正式 Debug 与 Release，两份产品隐藏短跑退出 0。V1–V4 重链后全部通过，V4 仍沿临时 Workspace 入口控制面板显隐。OpenSpec 严格校验通过。A 交接清单保留本轮实际源码、构建和运行版本，后续由 B 复核。 [V1–V3](../../../../build/d09-t01/a-correction-1/cpu-release.log) · [V4](../../../../build/d09-t01/a-correction-1/native-release.log) · [校验](../../../../build/d09-t01/a-correction-1/openspec-validation.json) · [交接](../../../../build/d09-t01/a-correction-1/handoff.json)

<a id="b-recheck-1"></a>
## B 首轮整改复核

- **版本与范围** 基准为 `df15ba4804cb847da25db2f6a4b89ca7c170da22`。A 整改版 1202 个源码输入、30 项工作区文件和 132 项验证材料均匹配清单。反向核对四个正式 C++ 文件的整改差异可还原上一轮 B 的源码指纹。本次 B 补注后源码指纹为 `c945731a45cafe1738d0a67aa22f3235ab51d6410b7069a7b4b7ff3a192b82e2`。 [输入与输出清单](../../../../build/d09-t01/b-recheck-1/review-inputs.json)
- **已通过** B 重跑原 CPU 边界、透视棱角、离屏陈旧布局、身份隔离及 V1–V4，共 6 个 Release 程序。CR-003 实测 5 面保留、1 面迁址、释放 2 个旧区域。CR-004 的拒绝与恢复均有当路摘要，主路票据继续推进。CR-005 在 D32 不可用时记录 4 灯拒绝。CR-006 区分了设备边长限制与额外字节降档。本次原生回放环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。 [逐面与计数](../../../../build/d09-t01/b-recheck-1/boundary.log) · [降档原因](../../../../build/d09-t01/b-recheck-1/receiver.log) · [离屏拒绝](../../../../build/d09-t01/b-recheck-1/offscreen.log) · [身份隔离](../../../../build/d09-t01/b-recheck-1/isolation.log)
- **仍开放** CR-001、CR-002 的原始反例已通过。B 另以视深 2 m 处的视锥棱点为接触点，用相邻两面的外法向正组合定义分离平面。将半径 1 m 的灯体沿该法向移开已知间隔，同一几何也以零间隔验证相切。两个灯体都在 8 m 以内。将相机远面从 8 m 分别改到 100000 m 和 10000 m 后，明确隔开 1 mm 的聚光和隔开 0.01 mm 的点光均错误获得区域，挤掉有覆盖的点光。正式 Release 库重链的探针退出码为 1。仅增加分支输出的几何实现副本将两次误分配都定位到 48 次支持查询耗尽。 [几何构造](../../../../build/d09-t01/b-recheck-1/ReceiverEdge.cpp) · [正式库回放](../../../../build/d09-t01/b-recheck-1/receiver-edge.log) · [诊断轨迹](../../../../build/d09-t01/b-recheck-1/receiver-trace.log)
- **注释与交接** 已补充有限灯体的支持点、分离判据和保守退出，逐面地址及内容代、布局拒绝归属和降档显示。四个文件的非注释源码与宏续行均与 A 整改版一致。沿 CR-001、CR-002 交回 A，5.3 保持待签收。 [当前问题单](../../../../build/d09-t01/final/review-signed.md)

<a id="a-correction-2"></a>
## A 第二轮整改

- 核对 B 补注后的 1202 个正式输入及 B 登记的棱边探针、参数、库、程序与日志。原 B 程序再次复现两个长远面误分配并退出 1。用同一探针源与修正版 Release 库重链后退出 0。 [整改前复现](../../../../build/d09-t01/a-correction-2/receiver-before.log) · [整改后重放](../../../../build/d09-t01/a-correction-2/ReceiverEdge.log)
- CR-001 用灯体可达的局部接收凸体检查聚光外锥。8 m 与 100000 m 远面下，1 mm 间隔均释放预算给有覆盖的点光，零间隔仍获准。CR-002 依据球心到有限接收体的最近距离，8 m 与 10000 m 远面下均拒绝相隔 0.01 mm 的球、保留相切球。额外核对实际端面截入灯体时的准入变化，以及 50000 m 视深处窄外锥的分离与相切。 [端面与远处用例](../../../../build/d09-t01/a-correction-2/ReceiverWindow.log)
- 原 `Boundary`、`ReceiverCases` 和 V1–V4 用当前 Release 重新链接并通过。Windows x64 正式 Debug、Release 构建、隐藏短跑和 OpenSpec 严格校验通过。V4 原生环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0，面板显隐沿用临时驱动。两项交还 B 独立复核。 [CPU 边界](../../../../build/d09-t01/a-correction-2/Boundary.log) · [V1–V3](../../../../build/d09-t01/a-correction-2/cpu-release.log) · [V4](../../../../build/d09-t01/a-correction-2/native-release.log) · [交接](../../../../build/d09-t01/a-correction-2/handoff.json)

<a id="b-recheck-2"></a>
## B 第二轮整改复核与签收

- **验收版本** `v2.0` 基于 `df15ba4804cb847da25db2f6a4b89ca7c170da22`。A 交接的 1202 个源码输入、30 项工作区文件及 129 项验证材料均匹配。与前一轮 B 版本相比，唯一改动的正式源码是 `Engine/Renderer/Lighting/ShadowVolumes.cpp`。B 补注后的签收源码指纹为 `a0773907d4b00520f45b3b0d88f8c3851b83ce3cc4bfb791a3e62dd48f83739a`。 [签收清单与输入身份](../../../../build/d09-t01/b-recheck-2/review-inputs.json)
- **CR-001、CR-002 通过** 上轮程序仍能复现远面延长后的两处误分配。用签收源码编译的几何对象重链同一棱边探针后，间隔 1 mm 的聚光在 8 m 和 100000 m 远面均被拒，间隔 0.01 mm 的点光在 8 m 和 10000 m 远面均被拒。两组中的有效点光都获得六面，相切灯都获准，完整可见灯数分别为 4、5。实际远面从 4 m 变为 5 m 使接收体进入灯球时，准入随之恢复。 [旧版负例](../../../../build/d09-t01/b-recheck-2/receiver-before.log) · [签收版棱边对照](../../../../build/d09-t01/b-recheck-2/signed-edge.log) · [实际远面及远处窄锥](../../../../build/d09-t01/b-recheck-2/receiver-window.log)
- **独立近面对照** 将相机旋转并平移，使用 1 m 的正交近面和 1 m 的灯球。点光中心到接收棱的距离依次约为 1.008 m、1.000 m、0.992 m，得到 0、6、6 层。背向聚光在近面外、锥尖触及近面、转向接收区域三个输入下得到 0、1、1 层。三次均保留 4 盏可见灯。 [复现源码](../../../../build/d09-t01/b-recheck-2/OrientedNear.cpp) · [实际输出](../../../../build/d09-t01/b-recheck-2/oriented-near.log)
- **回归与注释** B 重跑 A 登记的 6 个 Release 程序，覆盖原边界、棱角与 V1–V4。另用签收对象重链 V4 双 View 正常帧，在 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 32.0.101.6129 上通过，面板显隐由临时 Workspace 驱动。已在本轮实现中补明灯心相对视深、有限精度退出、最近距离驻点和截面转折的含义，非注释源码与宏续行核对一致。Debug 正式构建和启动沿用 A 的匹配记录。 [CPU V1–V3](../../../../build/d09-t01/b-recheck-2/v1-v3.log) · [签收对象原生 V4](../../../../build/d09-t01/b-recheck-2/signed-native.log) · [A 自检](#a-correction-2)
- **交接** CR-001 至 CR-006 全部关闭，B 签收 5.3。A 接续 6.1–6.3 最终验证、规范同步及归档。 [当前问题单](../../../../build/d09-t01/final/review-signed.md)

<a id="final-verification"></a>
## A 最终验证

核对 B 签收的 1202 个正式输入和 153 项登记证据。当前源码指纹为 `a0773907d4b00520f45b3b0d88f8c3851b83ce3cc4bfb791a3e62dd48f83739a`，六项 CR 和 B 任务 5.3 均已签收。问题单按二进制原样留存，原相对引用以 `Docs/CodeReview.md` 为来源。 [签收输入](../../../../build/d09-t01/final/signed-inputs.json) · [B 签收原件](../../../../build/d09-t01/final/review-signed.md)

| 规范条款 | 实现与已核对场景 |
|---|---|
| 有限阴影视图与保守投射物 | Lighting 使用稳定有限接收区间和冻结 Primitive 全表。V1 保留画外 caster 并核对局部灯射程。重链后，短／长远面与旋转相机近面的分离、相切对照均匹配 B。 [阴影视图](../../../../Engine/Renderer/Lighting/ShadowVolumes.cpp) · [棱边](../../../../build/d09-t01/final/SignedEdge.log) · [近面](../../../../build/d09-t01/final/OrientedNear.log) |
| 有界分配与灯索引 | V2 核对六面整体准入、兼容地址、字节预算、删灯和有效边长变化。合并 B 已验证的 8 转 7 层逐面迁址和 D32 能力拒绝控制。 [分配](../../../../Engine/Renderer/Lighting/ShadowPlan.cpp) · [V2](../../../../build/d09-t01/final/ShadowCpu.log) · [逐面迁址和计数](../../../../build/d09-t01/b-recheck-1/boundary.log) |
| 所需内容版本及复用 | V3 核对安全槽轮转、曝光和实际 jitter、caster 移出、MASK 内容变化、禁用及 Scene 更换。 [缓存](../../../../Engine/Renderer/Lighting/ShadowPlan.cpp) · [V3](../../../../build/d09-t01/final/ShadowCpu.log) |
| 每 View 的规划请求 | View 会话请求和共享 SceneFrame 通过同一正式帧入口准备。V4 核对双 View、非法距离、无效布局、恢复和禁用，复用 B 的陈旧布局及外来身份隔离证据。 [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [V4](../../../../build/d09-t01/final/SignedNative.log) · [陈旧布局](../../../../build/d09-t01/b-recheck-1/offscreen.log) · [身份隔离](../../../../build/d09-t01/b-recheck-1/isolation.log) |
| 规划与统计 | Runtime 在图尝试前发布 CPU 计划摘要。V4 核对图准入失败保留的票据、面板显隐和受光画面，四盏直接光完整参与。 [正式帧](../../../../Runtime/Frame.cpp) · [原生结果](../../../../build/d09-t01/final/SignedNative.log) |

5 条要求、9 个场景均有对应实现和证据。所有权、逻辑层预算、当前计划与接受输出的发布边界与 design 一致。Windows x64 正式 Debug／Release 重建，两个产品隐藏短跑均退出 0。4 个重链探针与 B 签收输出逐行一致。V4 的原生范围为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129，面板显隐由临时 Workspace 驱动。后续 D09 工作包负责 GPU 阴影生产和采样的验收。 [最终构建与运行身份](../../../../build/d09-t01/final/native-verify.json) · [同步前严格校验](../../../../build/d09-t01/final/pre-sync-validation.json)

<a id="spec-sync"></a>
## 规范同步

新增 `shadow-planning` 主规范的 3 条要求，在 `view-preparation` 和 `render-frame-statistics` 各追加 1 条。既有两个规格中的正文和场景保留，其他 38 份规范的文件内容匹配同步前的清单。重复比较三份增量均已同步。OpenSpec 1.13.2 严格校验通过 41 份主规范和当前 change。 [同步核对](../../../../build/d09-t01/final/spec-sync.json) · [同步后严格校验](../../../../build/d09-t01/final/synced-validation.json)

<a id="archive-closeout"></a>
## 归档

D09-T01 已按 OpenSpec 1.13.2 归档。开发计划保留验收链接，D09-T02 的前置已满足。签收原件留存后，当前问题单已按模板复位。 [归档回执](../../../../build/d09-t01/final/archive.json) · [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t01) · [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02)
