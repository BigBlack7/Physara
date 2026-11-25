# Tasks

## 1. 按预算生成完整或溢出的 cell

- [x] 1.1 A 按稳定 cell 顺序用剩余索引额度接纳完整记录，明确空格和 overflow 的 header，保全局灯及灯行身份。完成 V1，核对实际字段、紧邻预算边界和既有宽索引证据。 [列表及容量规格](specs/froxel-light-preparation/spec.md)

## 2. 拥有型上传及图内读取

- [x] 2.1 A 将当前 View 的参数和三张表纳入现有帧上传包，在封存前完成范围与额度检查，准备齐整后交付。用 V2 核对生成 stride、固定块范围、空逻辑表和初始来源借用结束后的载荷。 [交付设计](design.md#graph-delivery)
- [x] 2.2 A 声明各表的 `CopyDestination` 写版本和实际后继访问，沿现有图编译、上传准入及接受协议执行。完成 V2 的两路读回、消费活性和失败前缀恢复，核对两路版本与真正执行的上传。 [图交付规格](specs/froxel-light-preparation/spec.md) · [上传架构](../../../../Docs/Architecture.md#uploads)

## 3. 共享光照退路和诊断

- [x] 3.1 A 接通显式 Froxel 消费者的共享候选选择和全灯求值。用正式工具生成实际活动包并完成 V3，以原生 HDR 读回证明强制 overflow 及覆盖外查询与全灯结果等价，空正常格仍能读到全局灯。 [光照规格](specs/froxel-light-preparation/spec.md) · [光照聚合](../../../../Docs/Architecture.md#lighting)
- [x] 3.2 A 通过现有帧统计和诊断面板提供所属 View 的预算、overflow、有效索引和单次退路遍历量。用 V4 核对诊断来源、计量单位以及单路失败时的正常接受身份。 [诊断规格](specs/render-frame-statistics/spec.md)

## 4. A 集成自检与交接

- [x] 4.1 A 构建受影响的 Renderer、正式 Shader 包和产品入口，在原生 OpenGL 运行 V4 的普通 Forward 受光对照，确认活动绑定可用且无 Froxel 工作。严格校验 change，将实际命令、输入身份、观测结果、复用项及覆盖边界记录在本文件。 [构建说明](../../../../Docs/Build.md)
- [x] 4.2 A 在当前问题单登记基准及变更输入、包和可执行身份、夹具及可定位证据，交维护者调度独立 B。 [签收问题单](../../../../build/d10-t02/final/review-signed.md) · [审查流程](../../../../AGENTS.md#review-loop)

## 5. 独立审查、整改及复核

- [x] 5.1 B 独立核查预算及配对灯行、活动绑定、写读依赖、实际接受与 HDR 对照。在当前问题单记录发现及证据，并按规范完整检查本轮涉及的第一方文件注释。 [首轮审查记录](#b-review-1) · [注释约定](../../../../AGENTS.md#annotation)
- [x] 5.2 A 按问题编号整改并重跑受影响检查，将复现条件、修正输入和结果登记后交 B 复核。 [首轮整改记录](#a-correction-1) · [审查流程](../../../../AGENTS.md#review-loop)
- [x] 5.3 B 独立核对整改及注释，复查可区分错误的实际结果，签收对应源码与证据版本。 [B 签收记录](#b-recheck-1) · [签收问题单](../../../../build/d10-t02/final/review-signed.md)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后对照实现、预期规格和输入身份完成最终 verify。用匹配签收版本的构建及原生结果核对 D10-T02 完成条件，在本文件留存实际执行和复用证据。 [最终验证](#final-verification)
- [x] 6.2 A 留存签收证据，将两份增量同步至已有主规格，核对被修改的完整要求及新增场景，并严格校验同步结果。 [同步记录](#spec-sync)
- [x] 6.3 A 归档 change，校验归档链接、状态和计划，登记 D10-T02 完成及 D10-T03 前置。签收证据留存后按模板复位当前问题单。 [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)

<a id="verification-scope"></a>
## 验证口径与停止条件

| 检查 | 风险 | 最小充分验证 |
| --- | --- | --- |
| V1 · CPU 预算与 ABI | 部分列表、偏移重叠、零预算误拒绝、行号被窄化 | 用独立可数的几组有限灯让相邻格依次出现可容纳、超额、空格和恰好容纳，并混入无范围局部灯与方向灯。核对正常区间、overflow 格零占用、零额度下的全局列表及统计。从 D10-T01 复用计数超 255、偏移超 65,535 的夹具执行新增压紧路径。再用可定位的 header 超限或不可表示输入检查拒绝并恢复。 |
| V2 · 图内所有权、绑定和完成 | 源借用过期，绑定范围窄于反射，旧 View 内容混入新提交或失败后误确认 | 用显式消费者让两路不同的载荷进入同图。在 CPU 源借用结束后核对有区别的原生读回及写后读计划。将零有效索引或全局表编入同一检查，核对合法绑定和实际字节。移去一路消费时核对该路私有上传被裁剪。选一个上传后失败的前缀，检查没有接受新输出、实际完成前保活，以及后续正常候选正确读回。 |
| V3 · 原生 HDR 与退路分支 | overflow 被当空格，覆盖外漏光，全局灯被重复累加，阴影引用错行 | 在同一临时 Shader 包和原生光栅消费者中比较完整灯模式与混合预算的 Froxel 模式。固定同一输入、画布、相机、覆盖、材质、曝光、阴影、环境及 HDR 附件，利用独立已知贡献的有限点光、聚光、全局局部灯和方向灯区分行号。确认实际命中的正常格、空格、overflow 和覆盖外样点。对两种退路像素要求预曝光 RGBA16F 读回编码一致，并用去掉一盏有独立颜色贡献的有限灯的全灯对照证明差异可检测。 |
| V4 · 诊断和原有受光入口 | 混淆准备量与执行量、切换诊断导致额外工作、普通 Forward 活动绑定增长 | 复用 V1–V3 的预算结果、Graph 使用和提交对照，核对两路身份、绝对条目、单次退路局部灯行数和实际上传字节。检查只读面板显隐前后的工作及失败一路的接受记录。用正式 OpenGL 完整灯消费者执行一帧代表性受光图，确认没有 Froxel 准备、绑定及上传。 |

开发探针、临时包和证据放入 `build/d10-t02/`，通过现有编译入口和原生设备运行。本轮优先使用已知且可区分错误的输入，证据须对应实际源码、生成接口、Shader 包、设备和提交。网格几何谓词及查询边界可复用身份匹配的 D10-T01 证据。达到 V1–V4 且具体风险各有证据时结束检查。V3 的探针证明此处的局部灯退路合同，D10-T05 验收两条正式路径和成本。 [前置验收](../2026-11-25-prepare-cpu-froxel-light-lists/tasks.md#final-verification) · [验证规范](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## A 本地验证与交接

基准为 `f57c03f707fb7d4f2b71259e0cf77967ff9d2b44`。本轮使用 Windows x64、MSVC 19.44.35228、Windows SDK 10.0.26100.0。原生探针使用 Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 32.0.101.6129。

| 检查 | 实际结果 |
| --- | --- |
| V1 · CPU 预算与布局 | `Budget-Release.exe cpu` 重跑前置夹具和新增混合预算。320 格、单格 257 条，精确额度生成 82,240 条且末格偏移 81,983。额度少一条时只有末格 overflow。四格夹具额度为 2，依次得到正常 1 条、溢出、正常空格和正常 1 条，索引行 1 和 5，全局行 2。零额度时三格溢出，保留空格及全局灯。不可表示视域和头表超限后可恢复。 [CPU 日志](../../../../build/d10-t02/a/v1.log) |
| V2 · 拥有型上传及写读顺序 | 重新利用 CPU 工作区后，票据 1 的同图两路各读回自己的版本，分别使用 2 条和 0 条有限索引。图验证实际写区间、写版本和后继用途。去掉一路颜色消费后，票据 2 少执行四笔、2,163,292 字节。票据 3 只提交失败前缀，仍租用 3,145,728 字节，真实完成后归还。票据 4 重新准备，读得第 1 格的偏移 1、数量 3 和正常标记。实际 UBO 块可用 65,536 字节，607 字节的限制在分配前拒绝 608 字节块，上传批次超限保留已登记前缀。 [原生日志](../../../../build/d10-t02/a/native.log) |
| V3 · 共享光照退路 | 同一原生包的全灯与 Froxel 模式在两路各四个样点的 RGBA16F 编码一致。强制溢出像素的 RGB 为 9.75781、3.21484、0.56543。去掉有限绿光的对照降低 G，覆盖外去掉远端有限灯的对照降低 R。空正常格仍接收全局灯，实际 D32 硬阴影对左侧聚光遮挡，重试后正常格的正有限灯贡献与前轮退路一致。 [原生日志](../../../../build/d10-t02/a/native.log) · [验证片元源码](../../../../build/d10-t02/a/shaders/Probe.frag) |
| V4 · 正式受光和统计 | 用正式 Release 包运行正常 Forward，票据 5 两路实际出图，票据 6 只有第二路接受。首路保留票据 5 及当前布局拒绝，第二路保留票据 6，中心 HDR 编码相同。正式受光图中 Froxel 常量访问为零。诊断副本的预算、溢出、全局灯及退路遍历量和对应 View 可核对，面板显隐按现有只读调用点做源码检查。 [原生日志](../../../../build/d10-t02/a/native.log) · [面板调用](../../../../Editor/Panels/Workspace.cpp) |
| 正式构建和包 | Debug、Release 的 `Physara` 构建通过。Release `Physara.exe --hidden --poll-limit=15` 返回 0。11 个 StaticForward 系列包用生成 ABI 通过校验；原生探针验证所用片元的活动资源。 [Debug](../../../../build/d10-t02/a/build-debug-final.log) · [Release](../../../../build/d10-t02/a/build-release-final.log) · [启动](../../../../build/d10-t02/a/startup-result.txt) · [包校验](../../../../build/d10-t02/a/package-check.log) |

复现使用 `build/d10-t02/a/CompileProbe.ps1 -Probe Budget` 后运行 `Budget-Release.exe cpu`。原生回放用同一脚本以 `-Probe Native` 编译 `Native.cpp` 并链接本版 `Frame.obj`、`FrameStatistics.obj`、`Resources.obj` 及 Release 静态库，运行 `Native-Release.exe build/d10-t02/a/FroxelOverflow.pshader`。证据适用于列明的设备和受光夹具。D10-T03 接入正式 Forward+ 策略，D10-T05 验收跨路径与成本。 [构建命令和输入摘要](../../../../build/d10-t02/a/handoff.json) · [签收问题单](../../../../build/d10-t02/final/review-signed.md)

<a id="b-review-1"></a>
## B 首轮独立审查 · 2026-11-25

基准 `f57c03f707fb7d4f2b71259e0cf77967ff9d2b44`，分支 `v2.0`。审查前与 A 的清单比对，16 个实现及构建输入、19 项 Shader 镜像、9 个探针输入、53 个构建产物和 11 份日志的摘要匹配。B 为其中 13 个源码文件补充注释，比较了注释前后的有效代码和预处理指令。首轮发现 4 项，代码待整改，最终签收留在 5.3。 [问题与注释覆盖](../../../../build/d10-t02/final/review-signed.md) · [B 输入、产物和证据摘要](../../../../build/d10-t02/b-review/review-inputs.json)

| 复核对象 | B 实际执行与观测 |
|---|---|
| CPU 分箱与拒绝 | 重跑交接的 `Budget-Release.exe cpu`。320 格在恰好容纳时压入 82,240 条，少一条仅末格溢出，混合预算结果为正常、溢出、空、正常。另以 `FroxelStats-Release.exe` 运行 CR-004 的三种准备边界，已裁剪但附件拒绝时丢失可确定的 1 行局部灯数。 [CPU 对照](../../../../build/d10-t02/b-review/stats.log) |
| 图消费、原生读回与普通 Forward | B 探针链接清单中的 Release 库及 Runtime 对象，在同一翻译单元调用本版场景上传声明函数。重跑原有两路写读、移去一路少 4 笔和 2,163,292 字节、失败前缀租用到完成、重试、硬阴影和普通 Forward 的分路接受对照。上述原有输入通过。共享场景批次的第二次写入实际返回 `StaleWrite`（CR-001）；预算少 1 字节时原生缓冲创建数为 4（CR-002）。 [探针源码](../../../../build/d10-t02/b-review/Review.cpp) · [回放日志](../../../../build/d10-t02/b-review/annotated-native.log) |
| HDR 与注释后的 Shader | 以正式 ShaderBuild 和当前工作区 include 重建实验包。固定同一冻结场景、14,000 盏有限点光及 1 盏无范围点光，两个相邻额度让同一格分别正常与溢出。正常红通道为 `0x415a`，溢出和完整灯对照为 `0x4159`（CR-003）。实际运行 `Review-Release.exe build/d10-t02/b-review/FroxelOverflow.pshader`，探针按本轮预期复现断言退出 0。 [原生读回](../../../../build/d10-t02/b-review/annotated-native.log) |

设备和驱动同 A 记录，B 原生结果限于 Windows x64 Release。面板显隐按只读调用点核对；Debug 和正式产品构建复用 A 的交接证据。B 已对本 change 运行 OpenSpec 严格校验。A 在 5.2 逐项记录修正及回归后交回 B。

<a id="a-correction-1"></a>
## A 首轮整改与自检 · 2026-11-25

开始整改前核对 B 补注后的 16 个输入，基准 commit 和 SHA-256 均匹配。本轮依据 B 的原生及 CPU 夹具调整断言，将双路、裁剪、失败前缀和重试统一接入真实的 `DeclareUploads`。 [B 输入清单](../../../../build/d10-t02/b-review/review-inputs.json) · [A 整改清单](../../../../build/d10-t02/a-correction-1/handoff.json)

| 问题 | 实际回归结果 |
| --- | --- |
| CR-001 · 场景上传版本 | 单路共享帧批次恰有四个生产者，双路各四个且输出版本均为 1。读取复用原写版本和范围。票据 1 双路读回，票据 2 去掉一路后少四笔、2,163,292 字节。票据 3 的失败前缀租用 3,145,728 字节，完成后归零。票据 4 重新准备并读回第 1 格 `offset=1,count=3,overflow=0`，正有限灯贡献保留。 [原生日志](../../../../build/d10-t02/a-correction-1/native.log) |
| CR-002 · 早期额度及回滚 | 差一字节及差一笔时，在原生创建计数为 0 的情况下报告 `SceneUploadLimit`。预留后由真实 View 驻留额度拒绝时，批次恢复原有的 32 字节、1 笔。正好足够时创建四个目标，字节和笔数到达 67,108,864、4,096。 [原生日志](../../../../build/d10-t02/a-correction-1/native.log) |
| CR-003 · HDR 累加顺序 | 原冻结输入为 14,000 盏有限弱灯、一个表尾无范围亮灯，预曝光 0.75，额度为 14,000 与 13,999。票据 7 的正常、溢出和完整灯三组 RGBA16F 读回编码全部一致，R 均为 `0x4159`。空正常格和硬阴影行对照一并复跑。 [原生日志](../../../../build/d10-t02/a-correction-1/native.log) |
| CR-004 · 裁剪后诊断 | 布局早退时无可见灯计数，附件限制时保有两行可见灯及一次退路的 1 行局部灯，正常请求同样为 1。普通 Forward 跳过 Froxel 准备与统计。 [CPU 对照](../../../../build/d10-t02/a-correction-1/stats.log) |

Release 和 Debug 正式目标构建通过，Release 隐藏窗口执行 `--poll-limit=15` 返回 0。以本轮 Release 库重链预算 CPU 夹具，验证 82,240 条索引、宽偏移、精确与相邻额度、零额度和空格。正式 Forward 分路回放的接受票据仍按实际 View 分别记录，主路拒绝后保留票据 5，附加路推进到票据 6，中心 HDR 编码相同。本轮原生结果适用于日志登记的 Intel OpenGL 4.6 设备和驱动。 [Release](../../../../build/d10-t02/a-correction-1/build-release.log) · [Debug](../../../../build/d10-t02/a-correction-1/build-debug.log) · [启动](../../../../build/d10-t02/a-correction-1/startup-result.txt) · [预算回归](../../../../build/d10-t02/a-correction-1/budget.log)

<a id="b-recheck-1"></a>
## B 整改复核与签收 · 2026-11-25

**CR-001 至 CR-004 全部通过，代码及注释签收。** 基准为 `v2.0` 上的 `f57c03f707fb7d4f2b71259e0cf77967ff9d2b44`。B 比对了 A 整改清单中的 16 个实现输入、15 个探针输入、19 项镜像、54 个产物和 11 份日志，并核对了架构 8.6 与两份增量规格。相对 A 的交接，最终实现仅在 `FroxelGpu.cpp` 的异常回滚处增加一行说明。签收清单 SHA-256 为 `8aa35adf7e155148f41676de28d926142749c196453603f03cb5981d5f4f40c8`。 [逐项问题单](../../../../build/d10-t02/final/review-signed.md) · [签收输入及验证产物](../../../../build/d10-t02/b-recheck-1/review-inputs.json)

| 问题 | B 独立复核结果 |
|---|---|
| CR-001 · 共享上传 | 每路四块均取得同一场景批次的版本 1。两路保留时 8 笔 Froxel 写入，移去一路后为 4 笔，少传 2,163,292 字节。票据 3 的失败前缀在完成后归还上传区段，票据 4 读回新格的偏移 1、计数 3 和正常标记。 [原生回放](../../../../build/d10-t02/b-recheck-1/native.log) |
| CR-002 · 预算事务 | 字节与笔数分别少 1 时，原生缓冲创建均为 0。预留后驻留被拒，批次保留原有 32 字节、1 笔。额度恰足时创建四个缓冲，最终为 67,108,864 字节、4,096 笔。 [原生回放](../../../../build/d10-t02/b-recheck-1/native.log) |
| CR-003 · HDR 行序 | 14,000 盏有限弱灯和一盏表尾亮灯，预曝光 0.75。正常格、溢出格与全灯的 RGBA16F 编码逐通道一致，红通道均为 `0x4159`。空格、覆盖外、硬阴影和移去有效灯的对照继续通过。 [原生回放](../../../../build/d10-t02/b-recheck-1/native.log) |
| CR-004 · 退路行数 | 布局早退为未知；可见灯裁剪后被附件限制拒绝及正常准备均报告 1 行。普通 Forward 跳过 Froxel 摘要。宽索引检查读得单格 257 条、末格偏移 81,983，混合和零预算也通过。 [统计边界](../../../../build/d10-t02/b-recheck-1/Stats.log) · [CPU 分箱](../../../../build/d10-t02/b-recheck-1/Budget.log) |

B 直接编译工作区的 `Froxel.cpp`、`ViewPreparation.cpp`、`FroxelGpu.cpp` 并链接已核对的 Release 依赖。正式 ShaderBuild 从当前 include 生成的实验包与 A 的整改包逐字节一致。运行 `Recheck-Release.exe FroxelOverflow.pshader`、`Stats-Release.exe` 和 `Budget-Release.exe cpu` 均退出 0。正式 Forward 回放中，主路被拒后仍保留票据 5，附加路推进到票据 6。设备为 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。面板显隐按只读调用链核对，正式 Debug/Release 构建与产品隐藏启动复用 A 的整改证据。下一环节由 A 对签收输入完成 6.1–6.3。

<a id="final-verification"></a>
## A 最终验证 · 2026-11-25

B 的签收清单与工作区的 16 个实现输入、6 个合同文件、2 个实际共享消费者及 14 项复核产物均匹配。从签收源码重新构建 Debug 和 Release 的 `Physara`，Release 运行 `--hidden --poll-limit=15` 返回 0。11 个 StaticForward 系列包通过生成 ABI 校验。A 以本次 Release 正式库和 Runtime 对象重链原生探针，重新生成的实验包与 B 签收包逐字节一致，实际回放日志与 B 的 26 行原生结果一致。 [签收身份](../../../../build/d10-t02/final/signed-inputs.json) · [Debug](../../../../build/d10-t02/final/build-debug.log) · [Release](../../../../build/d10-t02/final/build-release.log) · [正式包](../../../../build/d10-t02/final/package-check.log) · [最终回放](../../../../build/d10-t02/final/native.log)

| 增量规格与场景 | 实现及验收 |
| --- | --- |
| 32 位载荷和逐格预算 · 3 个场景 | CPU 分箱只为完整候选划定区间。与签收输入匹配的 B CPU 回放覆盖单格 257 条、末格偏移 81,983、混合与零预算，以及超限后恢复。 [分箱](../../../../Engine/Renderer/Lighting/Froxel.cpp) · [复用 CPU 证据](../../../../build/d10-t02/b-recheck-1/Budget.log) |
| 拥有型图上传 · 2 个场景 | 共用帧批次声明四个写版本，后继读取同一路的精确写范围。票据 1 两路均读回，票据 2 裁掉一路后少四笔、2,163,292 字节。失败前缀票据 3 的上传区段按完成退休，票据 4 读取重试记录。差一字节与差一笔均在原生分配前拒绝。 [场景上传](../../../../Engine/Renderer/Paths/Forward.cpp) · [Froxel 图版本](../../../../Engine/Renderer/Lighting/FroxelGpu.cpp) · [最终回放](../../../../build/d10-t02/final/native.log) |
| 共享光照退路 · 3 个场景 | 强制溢出和覆盖外样点与全灯对照逐位一致，空格仍消费全局局部灯。票据 7 的 14,000 盏弱有限灯与一盏表尾亮灯用相邻预算比较，正常、溢出和全灯的 RGBA16F 编码相同，红通道均为 `0x4159`。 [候选归并与退路](../../../../Shaders/Lighting/FroxelLighting.glsl) · [最终回放](../../../../build/d10-t02/final/native.log) |
| 按 View 诊断 · 2 个场景 | CPU 计数随已裁剪工作集发布。复用 B 签收的布局早退、附件拒绝和正常准备三种结果，退路行数依次为未知、1、1。最终原生回放确认两路普通 Forward 票据 5 与 6 独立保留。面板显隐以只读调用链核对。 [View 统计](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [统计面板](../../../../Editor/Panels/Workspace.cpp) · [复用 CPU 证据](../../../../build/d10-t02/b-recheck-1/Stats.log) |

4 条增量要求和 10 个场景均核对到实现及证据。归档范围为 D10-T02，原生回放对应 Windows x64 Release、Intel RaptorLake-S Mobile 及 OpenGL 4.6.0 驱动 32.0.101.6129。 [实际构建和复用证据清单](../../../../build/d10-t02/final/native-verify.json)

<a id="spec-sync"></a>
## 规格同步

在既有 `froxel-light-preparation` 更新完整 32 位载荷要求，加入拥有型图上传和全灯退路两条要求。在既有 `render-frame-statistics` 加入 Froxel 按 View 的预算与遍历量要求。两份主规格保留原 Purpose 和既有未涉及要求，43 份主规格均通过严格校验。 [Froxel 主规格](../../../specs/froxel-light-preparation/spec.md) · [帧统计规格](../../../specs/render-frame-statistics/spec.md) · [合并身份](../../../../build/d10-t02/final/spec-sync.json) · [严格校验](../../../../build/d10-t02/final/validate-synced-specs.json)

<a id="archive-closeout"></a>
## 归档与阶段推进

D10-T02 已验收并归档，开发计划推进至 D10-T03。签收问题单按原始字节留存，链接以原 Docs/CodeReview.md 所在目录为基准。 [签收问题单](../../../../build/d10-t02/final/review-signed.md) · [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)
