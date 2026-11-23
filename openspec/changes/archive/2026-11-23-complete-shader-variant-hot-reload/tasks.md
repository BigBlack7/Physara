# Tasks

## 1. 程序目录、变体与 CPU 包

- [x] 1.1 将现有九个产品角色登记为有限目录，提供规范化 `VariantKey` 及完整组合解析。核对打包入口的角色和现有 ABI，检查同语义请求的稳定键、一个单字段合法但组合非法的对照，以及连续材质或曝光变化沿用同一变体。非法组合在进入 CPU 或原生准备前报告原因。 [变体设计](design.md#variant-catalog) · [Shader 构建规范](specs/shader-build/spec.md)
- [x] 1.2 复用 Engine/Shader 的离线生成、传递依赖、编译与反射合同，为运行时包缓存核对完整输入并执行条目及逻辑字节预算。用两个现有程序证明未变化输入命中、只修改一项传递 include 时仅相关键改变、不同工具或 ABI 身份不能误命中。用缩小额度检查保护现有包及明确的容量拒绝。 [CPU 准备设计](design.md#source-notification) · [预算](design.md#preparation-budget)
- [x] 1.3 接通包与已验证 CPU 数据的共同 RHI Shader 消费入口，保留现有离线包消费者。以同 ABI 的两个真实光照或显示角色核对成功匹配及错包拒绝，记录 ABI、阶段和活动资源核对的失败位置。若构建依赖或生成规则改变，验证空 binary tree 到产品消费者的顺序。 [既有包消费规范](../../../specs/opengl-shader-pipelines/spec.md)

## 2. Context 线程准备与完整代发布

- [x] 2.1 接入原生缓存和有界准入，采用当前设备、驱动、实际绑定映射及完整 Pipeline 兼容键。用真实 OpenGL 验证不同附件身份的同兼容请求复用，改变实际输入布局或目标格式采用新键。以受控键对照验证驱动身份隔离，以小额度验证持有对象上的容量压力，记录实际 link、创建和命中。 [原生缓存规范](specs/opengl-shader-pipelines/spec.md)
- [x] 2.2 把 Forward 几何、天空及显示的首次读包和 Pipeline 获取接入帧前推进，在设备 owner context 按额度执行。用一份需多个 Pipeline 的合法计划及每轮一个新创建的预算，确认每轮计数、下轮续办与 Pending 状态。跟踪构图和实际 Draw 的原生创建计数，验证它们只消费就绪项。 [准备预算](design.md#preparation-budget)
- [x] 2.3 持有完整候选所需程序、布局、绑定配方及资源代，于匹配当前请求且全数就绪的帧边界发布。用同源的主视图和离屏准备核对最后一项就绪前的旧代及发布后的单一新代。以一次明确的活动绑定或 CPU ABI 不兼容和一次目标相关原生准备失败，确认旧完整代、首错及临时对象收拢。 [代发布规范](specs/shader-generation-reload/spec.md) · [代发布设计](design.md#generation-publication)

## 3. 帧消费、旧代与退出

- [x] 3.1 在 Runtime 构图前冻结就绪代和各 View 计划，并沿正常图或安全 UI 保留可用画面。用已有双 View 和池压力入口核对 Pending 时的选择、匹配正常图接受后才确认有效输出，以及未被接受的出口和安全 UI 保留上一 View 结果。 [帧规范](specs/editor-frame-graph/spec.md) · [受保留资源规范](../../../specs/render-graph-resource-pool/spec.md)
- [x] 3.2 将旧代及其布局、绑定、Pipeline 传递到帧槽和既有录制依赖的实际寿命。让上一代在本次准备时仍有受控 Pending 工作，验证发布新代后保护旧对象和正常退休。关闭时补验仍有候选或待完成包的路径，保留真实完成观测及 context 清理顺序。 [退休合同](../../../../Docs/Architecture.md#retirement) · [关闭合同](../../../../Docs/Architecture.md#shutdown)

## 4. 文件通知、后台构建与错误恢复

- [x] 4.1 接入显式开发源码根和 Windows 变更通知，以有界集合合并修改、原子保存和无法穷举的事件。从临时源码树重放一次实际 include 替换保存，核对归并范围及两项下游程序中受影响一方的身份。校验不可用源码根的诊断和打包启动的正常基线。 [通知设计](design.md#source-notification)
- [x] 4.2 使用单 Compiler worker 推进自持 CPU 请求，核对源闭包、请求身份和最新需求，按既有协议取消及退出。在首次作业返回前提交两次连续修改并控制返回顺序，验证只保留最新候选，旧回执无法触发发布或覆盖诊断。关闭时检查通知和 worker 收拢、回执丢弃及原生 owner 次序。 [异步候选规范](specs/shader-generation-reload/spec.md)
- [x] 4.3 将实际文件编辑接入完整代通路。用传递 include 的可定位语法错误观察 Failed、原文件行号与旧色，再修复并改变一个具有确定预期的像素，核对新编译、预算准备、帧边界发布和可靠提交后的图像。与 3.2 的旧代寿命证据关联并注明真实与受控条件。 [失败与恢复合同](specs/shader-generation-reload/spec.md)

## 5. 诊断和 A 本地集成验收

- [x] 5.1 由 Shader 及原生准备 owner 发布状态、缓存占用、命中、真实编译、link 和准备耗时，沿现有统计快照供 Editor 展示。以额度顺延后失败或恢复的同一序列核对两类事实及源位置。比较面板显示与隐藏时的工作次序、创建次数和最近接受场景的票据。 [统计规范](specs/render-frame-statistics/spec.md)
- [x] 5.2 构建 Debug、Release 的受影响模块与产品，沿打包入口运行隐藏启动，核对真实双 View、热重载和安全 UI 证据。复用输入身份匹配的检查，记录源、包、ABI、库、程序、执行命令、设备与限制。运行 OpenSpec 严格校验，核对链接与工作区差异。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md)

本地辅助输入和日志按工作包放在 `build/d08-t07/`。实际通过后再在对应项登记证据。D08-T08 负责同口径执行成本及空闲测量。

<a id="a-local-verification"></a>
### A 本地验证记录

- **程序和 CPU 包。** 九个产品角色与许可组合通过回放，非法组合和重复定义被拒绝。Debug 与 Release 的编译指纹及包内容身份不同，ABI 以同一声明核对。实际替换一个传递 include 后，三个依赖程序重新编译，其余六个命中。相同内容再次保存命中九个包。两条目对照保持限额并拒绝不完整候选。 [CPU 回放](../../../../build/d08-t07/a/ReloadCpu-Release.log)
- **原生准备。** 两种配置在真实 OpenGL 上通过。不同尺寸的 View 复用兼容 Pipeline，线性中间附件与 sRGB 终点分别准备，暖帧调整曝光沿用同代和原键。独立缓存域的一槽对照给出容量等待，旧色保持可读。以实际 GLSL link 入口确认每轮最多启动一次原生工作，View 准备、构图及 Draw 消费已就绪对象。 [Debug](../../../../build/d08-t07/a/RuntimeReload-Debug.log) · [Release](../../../../build/d08-t07/a/RuntimeReload-Release.log)
- **完整代与提交。** 双 View 在准备中保持旧代，通过匹配正常图提交共同确认新代。源码错误、ABI 不兼容和受控原生 link 拒绝保留旧代及首错。对实发旧代的 fence 定向报告 Pending，观测新代发布后旧帧槽仍持有旧代。恢复真实完成观察并轮换槽后，旧 Pipeline 容量归还。修复后实际读回中心像素为 `137,0,0`，旧保留输出像素一致。安全 UI 与零容量对照通过。 [原生回放](../../../../build/d08-t07/a/RuntimeReload-Release.log)
- **通知和界面。** 回放使用 Windows 替换保存、真实编译和恢复。诊断保留传递 include 的原文件行号，两个连续修订后的受控旧回执无法准备或发布。关闭对照在最新请求提交给队列后收拢会话。面板显示候选和最近接受场景的各自身份，读取快照期间原生命令和准备计数保持。打包、可用工作区及不可用源码根的隐藏入口均通过。 [源根启动](../../../../build/d08-t07/a/smoke-source-root.log) · [不可用根启动](../../../../build/d08-t07/a/smoke-invalid-root.log) · [面板及代回放](../../../../build/d08-t07/a/RuntimeReload-Release.log)
- **构建与边界。** Debug、Release、空 Ninja binary tree 下的 MSVC 产品构建和隐藏启动通过。20 个实际产品包已逐一核对 ABI 与可读取性，OpenSpec 1.13.2 严格校验 40 项通过。原生环境为 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。旧回执、link 拒绝及完成延迟包含受控操作。早期 MSVC `C1001` 和 Visual Studio 空目录 SDK 探测日志均保留。 [源码与证据身份](../../../../build/d08-t07/a/handoff.json) · [严格校验](../../../../build/d08-t07/a/openspec-strict.json)

## 6. 独立 Review/Annotation 闭环

- [x] 6.1 Agent A 完成本地检查后在当前问题单提供基准 commit、正式输入、未跟踪源码、运行入口和证据身份，注明复用及未覆盖范围。 [当前问题单](../../../../Docs/CodeReview.md) · [A 本地验证](#a-local-verification)
- [x] 6.2 Agent B 独立核对实际变化、受影响合同及直接消费者，补齐第一方文件的职责、接口及流程注释。验证覆盖变体键、快照发布、预算及旧代寿命，在问题单填写 CR、证据及注释覆盖。 [独立审查闭环](../../../../AGENTS.md#review-loop) · [B 初审记录](#b-initial-review)
- [x] 6.3 Agent A 按 CR 整改并运行能区分修正前后的最小复验。在问题单登记修正、实际结果及新交接版本，勾选 A 自检项。 [A 第一次整改](#a-correction-1) · [A 第二次整改](#a-correction-2) · [A 第三次整改](#a-correction-3)
- [x] 6.4 Agent B 独立复核本版整改、受影响注释和匹配的运行证据。全部正式问题关闭后记录签收的源码身份和边界。 [B 第一次整改复核](#b-recheck-1) · [B 第二次整改复核](#b-recheck-2) · [B 第三次整改复核与签收](#b-recheck-3)

<a id="b-initial-review"></a>
### B 第一次独立审查

B 已核对分支、基准、实际源码差异及 34 项正式输入。初审未通过，CR-001 至 CR-010 待 A 整改。四项 P1 涉及本帧 View 需求、深度 Pass 的实际纹理读取、原生链接期间的源码修订，以及发布后缓存计数分配失败。其余六项涉及原生键复用、独立 View 失败、重试、工作计数、错误源路径及单配置工具指纹。 [当前问题单与注释覆盖](../../../../Docs/CodeReview.md)

B 在 NVIDIA RTX 4080 Laptop / OpenGL 4.6.0 上使用 A 的 Release 库完成定向观测，并分别用两组编译选项配置 Ninja/MSVC。双 View 像素与在途 fence 证据引用上方 A 的 Intel 回放。B 在 29 个文件补充注释，34 项正式输入的注释后指纹为 `95812d282b2807379b67e9bf026e18d5a4634268ccaf2cf9adf00c03d88a37f0`，这是交回 A 的版本。 [输入与产物](../../../../build/d08-t07/b-review/evidence.json) · [原生观测](../../../../build/d08-t07/b-review/native-review.log) · [配置对照](../../../../build/d08-t07/b-review/flags.log)

<a id="a-correction-1"></a>
### A 第一次整改与自检

整改前核对基准 `cc0345a8439e0833bfbb4772f3da347084881507`、B 注释后的全部 34 项正式输入及 10 个 B 证据产物。整改版纳入帧前准备入口的声明 `Runtime/Frame.hpp`，共 35 个正式输入，源码指纹为 `eaeededdf1aedc69c28d88cf682bdb92cfdaf36b2b4fcbbeb5d06d1815a44897`。保留同一问题单和 CR 编号，A 自检栏和 6.3 已完成，待 B 核对代码、注释与证据。 [整改交接](../../../../build/d08-t07/a-correction-1/handoff.json) · [当前问题单](../../../../Docs/CodeReview.md)

| 发现 | 整改回放 |
| --- | --- |
| CR-001、CR-003 | 先按仅含 Clamp 的旧需求预热，再在当前帧启用离屏 FXAA。活动绑定不匹配时两路仍提交第 3 代，修复后共同接受第 4 代。最后一次真实 link 内触发 Windows 替换保存，新修订编译失败后第 4 代及原像素仍可用。 [Release](../../../../build/d08-t07/a-correction-1/RuntimeReload-Release.log#L27) · [Debug](../../../../build/d08-t07/a-correction-1/RuntimeReload-Debug.log#L27) |
| CR-002 | 同 ABI 的深度片元启用独立法线图后在 link 前拒绝。以独立基色、法线纹理执行旧代的 `earlyDepth=true` 图，深度批次存活且完成提交，中心读回 `255,0,0`。 [深度回放](../../../../build/d08-t07/a-correction-1/ReviewCorrections-Release.log#L10) |
| CR-004、CR-005 | 分配候选表失败时旧代、包、句柄及计数一致。旧的发布后分配注入点未触发，冻结表与新代共享句柄。单槽、32 槽各仅 1 次 link 和 1 个占用，格式变化触发另一键。 [原生对照](../../../../build/d08-t07/a-correction-1/ReviewCorrections-Release.log#L2) |
| CR-006、CR-007 | 主路 Grade 失败后保留原输出，离屏 Clamp 从 ShaderPending 到独立票据。同一请求的失败仅尝试一次，新修订的相同内容只补一次 link；退出并重选计划也可恢复。 [双路提交](../../../../build/d08-t07/a-correction-1/RuntimeReload-Release.log#L32) · [失败去重](../../../../build/d08-t07/a-correction-1/ReviewCorrections-Release.log#L7) |
| CR-008、CR-009 | 两阶段暖链取得 2 个命中且未调用 link。缺 `version` 的程序声明增加 1 次构建尝试、0 个 GLSL 阶段，包含声明路径和字段。实际 include 错误保留原始 GLSL 行号。 [计量与诊断](../../../../build/d08-t07/a-correction-1/ReviewCorrections-Release.log#L8) · [CPU 编译](../../../../build/d08-t07/a-correction-1/ReloadCpu-Release.log) |
| CR-010 | 在隔离 Ninja/MSVC 树构建 ForwardDisplay 包，`/fp:precise` 与 `/fp:strict` 的工具指纹及包摘要不同，ABI 一致。相同 precise 生成的身份头及包字节稳定。 [工具与包对照](../../../../build/d08-t07/a-correction-1/flags.log) |

正式 Debug、Release 构建及打包隐藏启动均通过，有效和不可用开发源根的启动各验证一次。20 个已构建产品包按各配置的消费者 ABI 检查。双 View 回放还覆盖安全 UI、实际像素 `137,0,0`、在途旧槽保护及事后释放容量。完成延迟、链接拒绝和候选分配失败为受控条件，其余编译、OpenGL link、图提交与读回使用真实实现。A 本轮原生环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。首轮 NVIDIA 结果保留为整改前证据。 [执行入口与票据](../../../../build/d08-t07/a-correction-1/runs.json) · [严格校验](../../../../build/d08-t07/a-correction-1/openspec-strict.json)

<a id="b-recheck-1"></a>
### B 第一次整改复核

B 匹配了 A 的 35 项正式输入、相对上次 B 交接的 19 项变化、6 个未跟踪源码路径和登记的 diff、库、包及运行程序。被复核源码指纹为 `eaeededdf1aedc69c28d88cf682bdb92cfdaf36b2b4fcbbeb5d06d1815a44897`。B 补齐 7 个文件的注释，交回指纹为 `5e146b2abb9278c67263bd6f445d2ef5f4e85eafe9c1d377e21d0482e5a74d99`。本次未签收。 [当前问题单](../../../../Docs/CodeReview.md) · [B 版本与证据](../../../../build/d08-t07/b-recheck-1/evidence.json)

| 处置 | 发现与复核结果 |
|---|---|
| 复核通过 | CR-001、002、003、004、006、008、010。Debug 与 Release 双 View 均在同一正常图接受修复后的第 4 代，最后一次 link 内的新保存未发布过期候选。独立纹理的深度预通过保留旧代并读回 `255,0,0`。候选分配、显示失败隔离、编译计数及三份 Ninja 包的身份对照通过。 [双 View Release](../../../../build/d08-t07/b-recheck-1/RuntimeReload-Release.log) · [双 View Debug](../../../../build/d08-t07/b-recheck-1/RuntimeReload-Debug.log) · [深度与计量](../../../../build/d08-t07/b-recheck-1/ReviewCorrections-Release.log) · [包校验](../../../../build/d08-t07/b-recheck-1/flag-check.log) |
| 仍开放 | CR-005 在旧代尚被持有时撤销编辑，未复用已计费的同键句柄。CR-007 的新源码候选原生失败后，退出并重选计划无法重试。CR-009 的合法 JSON 在目录组合不获准时仍缺来源路径和冲突字段。上述三项的 A 原用例均已独立通过。 [B 补充对照](../../../../build/d08-t07/b-recheck-1/boundaries-final.log) |
| 新发现 | CR-011 [P1] 首次物理显示在第 3 代已报告 Active 后才遇到固定 LUT 分配失败。CR-012 [P2] 两个原生键失败后收缩为一路，仍在使用的失败键丢失角色和原始原因。 [B 定向源码](../../../../build/d08-t07/b-recheck-1/Boundaries.cpp) · [实测记录](../../../../build/d08-t07/b-recheck-1/boundaries-final.log) |

四个登记回放在独立目录退出 0。B 本次使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129；冻结表强引用、原生 link 拒绝及 LUT 分配失败属于受控条件。A 的 Ninja 构建产物由 B 按配置身份及 ABI 再次校验。当前五项开放，6.4 待下次复核。 [CPU 源通知与原行号](../../../../build/d08-t07/b-recheck-1/ReloadCpu-Release.log) · [输入、命令和日志](../../../../build/d08-t07/b-recheck-1/evidence.json)

<a id="a-correction-2"></a>
### A 第二次整改与自检

A 在改动前逐项核对 B 交回的 35 个正式输入、6 个未跟踪源码路径、基准及 19 个登记产物。自检版的源码指纹为 `c09c01fa7bc39f1547b6bdef3d3ae7449a4eb8ba554cf32eca268bdecf800edf`，正式输入路径与基准相同，11 个源码文件与交回版有差异。五项自检结果已登记，待 B 独立复核。 [A 交接清单](../../../../build/d08-t07/a-correction-2/handoff.json) · [当前问题单](../../../../Docs/CodeReview.md)

| 发现 | 整改复验 |
| --- | --- |
| CR-005 | 实际修改 Clamp 后完整撤销。容量为 2 和 32 时均从冻结旧表接续同一 Pipeline，新增 link 为 0。旧表解除持有即可退休，复用对象仍由 Active 保有。 [撤销与退休](../../../../build/d08-t07/a-correction-2/Boundaries-Release.log#L2) |
| CR-007 | 一次性原生链接拒绝释放候选表并保有原 CPU 包。持续选择同一键和重复递交回执均无重试，切走后重选同一修订补一次 link，新内容随第 3 代生效。 [候选重试](../../../../build/d08-t07/a-correction-2/Boundaries-Release.log#L4) |
| CR-009 | 将真实请求的 `PHYSARA_PHYSICAL_LIGHTING` 从 1 改为 2。回执在 GLSL 编译前定位程序声明、实际值和获准值。缺 version 的预检及 include 的原始行号继续通过。 [目录校验](../../../../build/d08-t07/a-correction-2/Boundaries-Release.log#L8) · [原始行号](../../../../build/d08-t07/a-correction-2/ReloadCpu-Release.log) |
| CR-011 | 首次 LUT 像素分配故障发生在规划，已提交的第 2 代保有相同票据和像素，独立 Clamp 路继续提交。资源恢复后先取得 LUT 及可上传载荷，再发布第 3 代。真实图接受一笔 LUT 上传，下一帧沿原图状态合同导入。 [显示资源回放](../../../../build/d08-t07/a-correction-2/Boundaries-Release.log#L7) |
| CR-012 | Clamp 和 Grade 各保留自己的初始化错误。需求收缩时，Clamp 的角色与原 ABI 错误仍出现在快照和本路结果，新的准备次数为 0。 [诊断回放](../../../../build/d08-t07/a-correction-2/Boundaries-Release.log#L5) |

Debug、Release 产品构建及隐藏启动通过，逐一校验本版 20 个实际包的消费者 ABI。使用本版产物运行九个回放和启动检查，其中双 View Debug/Release 继续覆盖准备额度、过期候选拒绝、旧槽保护及退休、安全 UI 和像素读回。定向回放使用 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129，旧表持有、单次原生链接拒绝和首次 LUT 分配失败由探针控制。单配置 flags 的对照沿用 B 第一次复核。 [运行清单](../../../../build/d08-t07/a-correction-2/runs.json) · [Debug 双 View](../../../../build/d08-t07/a-correction-2/RuntimeReload-Debug.log) · [Release 双 View](../../../../build/d08-t07/a-correction-2/RuntimeReload-Release.log) · [OpenSpec strict](../../../../build/d08-t07/a-correction-2/openspec-strict.json)

<a id="b-recheck-2"></a>
### B 第二次整改复核

B 独立核对本轮 35 个正式输入、6 个未跟踪源码路径、11 个相对上次交接有变化的文件，以及登记的差异、库、包和回放程序。被复核源码指纹为 `c09c01fa7bc39f1547b6bdef3d3ae7449a4eb8ba554cf32eca268bdecf800edf`。B 在 4 个文件补充注释，交回指纹为 `7c5c4f03f0888e3436216493d5310f86e9b6bd17e751bab3067d32c025b12058`。 [当前问题单](../../../../Docs/CodeReview.md) · [B 版本与运行记录](../../../../build/d08-t07/b-recheck-2/evidence.json)

- **本轮通过 CR-005、007、009、011、012。** 撤销编辑在 2 槽及 32 槽均复用原句柄，新增 link 为 0。同一源码候选的失败键重选后只补一次 link。目录冲突带原路径、实际值与获准值。首次 LUT 分配失败保住第 2 代，恢复后第 3 代正常图只上传一笔 LUT。需求收缩后仍保留当前键的原始诊断。此前已通过的七项继续有效。 [五项定向回放](../../../../build/d08-t07/b-recheck-2/Boundaries-Release.log)
- **本轮新增 CR-013 [P2]。** 空场景先收到请求 1 的 CPU 包时，首次设备诊断表预留失败会丢失该回执。受控失败组在设备恢复后的六帧持续 Pending，打包第 1 代读回 `36,36,36`。正常组使用源码第 2 代，读回 `138,14,14`。失败组重新保存同样的字节后才取得相同颜色。本版未签收，当前仅此项待 A 整改。 [对照源码](../../../../build/d08-t07/b-recheck-2/ColdDelivery.cpp) · [实测](../../../../build/d08-t07/b-recheck-2/cold-delivery.log)

B 在 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 的隔离工作目录运行了 5 个登记回放，均退出 0。Debug 与 Release 双 View、Release 深度图及 CPU 源位置回归继续通过。CR-013 使用本版 Release 库及 Runtime 入口，真实执行编译、图提交和读回，只控制一次 owner 线程的分配失败。单配置 Ninja flags 的结果沿用第一次复核。 [双 View Release](../../../../build/d08-t07/b-recheck-2/RuntimeReload-Release.log) · [双 View Debug](../../../../build/d08-t07/b-recheck-2/RuntimeReload-Debug.log) · [深度图](../../../../build/d08-t07/b-recheck-2/ReviewCorrections-Release.log) · [CPU 源通知](../../../../build/d08-t07/b-recheck-2/ReloadCpu-Release.log)

<a id="a-correction-3"></a>
### A 第三次整改与自检

A 匹配 B 交回的 35 个正式输入、6 个未跟踪源码路径及 21 个登记产物。本次修正两个 Forward 文件，源码指纹为 `88a646ea8ff63d547d5b58ac16cf2fbda7518aa1ce24f0857920c2c175d58419`。CR-013 的 A 自检已完成，待 B 复核。 [当前问题单](../../../../Docs/CodeReview.md) · [本轮交接](../../../../build/d08-t07/a-correction-3/handoff.json)

- **CR-013 冷启动。** 回放沿 Runtime 帧前入口接收真实 CPU 包。故障组只拒绝首次 Forward 设备诊断表的分配，快照保留请求 1 和失败原因。设备随有效 View 规划恢复后，该修订继续发布为第 2 代。正常组和故障组都完成图提交并读回 `138,14,14`。恢复使用原回执，两组各执行 18 个 GLSL 阶段。 [定向回放](../../../../build/d08-t07/a-correction-3/ColdDelivery-Release.log#L2)
- **CR-013 修订替换。** 在故障后观察较新的 Windows 保存，再触发设备恢复。新回执到达前只能取得可用的打包第 1 代，随后第 2 代读回新源码的 `14,14,138`。递延的旧请求不会成为候选。 [修订对照](../../../../build/d08-t07/a-correction-3/ColdDelivery-Release.log#L8)
- **验证边界。** Debug/Release 产品构建、隐藏启动、有效与不可用开发根启动及本版双 View 原生回放通过，共 7 个执行检查。校验本版 20 个产品包的消费者 ABI。定向失败是受控条件，链接、图提交和像素读回使用真实 Intel OpenGL。RHI 深度、CPU 诊断和 Ninja flags 对照沿用 B 已核对的证据。 [执行记录](../../../../build/d08-t07/a-correction-3/runs.json) · [OpenSpec strict](../../../../build/d08-t07/a-correction-3/openspec-strict.json)

<a id="b-recheck-3"></a>
### B 第三次整改复核与签收

**B 已签收 D08-T07。** CR-001 至 CR-013 全部关闭，代码正确性与注释可读性均通过。签收分支为 `v2.0`，基准为 `cc0345a8439e0833bfbb4772f3da347084881507`。35 项正式输入包含 6 个未跟踪源码文件，暂存区为空。按逐文件路径和 SHA-256 计算的签收指纹为 `88a646ea8ff63d547d5b58ac16cf2fbda7518aa1ce24f0857920c2c175d58419`，与 A 第三次交接及 B 独立重放的输入一致。 [签收清单与命令](../../../../build/d08-t07/b-recheck-3/evidence.json)

本次仅 `Forward.cpp`、`Forward.hpp` 相对上次 B 交回版发生变化。B 核对了冷启动回执在 Renderer 与设备准备器之间的唯一持有关系、失败后的触发点及更新修订的失效规则。两份文件现有的接口和流程注释与实现一致。此前各项修正及注释覆盖已留存于[初审](#b-initial-review)、[第一次复核](#b-recheck-1)和[第二次复核](#b-recheck-2)。

| CR-013 的独立对照 | 实际结果 |
|---|---|
| 正常冷启动 | 快照在编译期间显示当前请求号。请求 4 使用 18 个 GLSL 阶段发布第 2 代，中心像素为 `138,14,14`。 |
| 首次设备分配失败一次 | 失败及空闲轮询期间保留请求 4 与分配原因。随后规划有效 View，在同一修订、原 18 个阶段的条件下发布第 2 代，像素与正常组相同。 |
| 恢复前观察到新修订 | 旧请求 3 的回执失效；新包抵达前使用打包第 1 代，随后只有请求 5 的蓝色源码发布为第 2 代，中心像素为 `14,14,138`。 |

B 在 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 上运行 Release 冷启动和 Debug/Release 双 View，共三个独立回放，均退出 0。双 View 继续覆盖同帧切代、安全 UI、在途旧代退休和 `137,0,0` 的真实读回。分配拒绝和保存时序采用受控条件；编译、原生 link、图提交和读回均使用实际实现。本版产品隐藏启动采用 A 的对应记录，独立深度图、CPU 诊断和单配置 flags 对照沿用先前复核。任务 7.1 至 7.3 由 A 对签收版本完成。 [冷启动](../../../../build/d08-t07/b-recheck-3/ColdDelivery-Release.log) · [双 View Release](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log) · [双 View Debug](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Debug.log) · [A 第三次交接](#a-correction-3)

<a id="final-verification"></a>
## 7. 最终 verify、规范同步与归档

- [x] 7.1 Agent A 针对 B 签收版本核对 D08-T07 完成条件、五份增量规范与设计，沿正式 Windows/OpenGL 入口确认匹配的原生证据。复用可证明对应签收输入的回放，补验具体缺口并留存 CR、注释覆盖、最终输入与边界。 [签收版原生验证](#signed-native-verification)
- [x] 7.2 将五份已验收增量规范同步到主规范，逐条核对新增合同与既有要求及场景的保留，运行全量主规范严格校验。 [主规范同步](#spec-sync)
- [x] 7.3 用项目锁定的 OpenSpec 流程归档，核对归档工件、链接、任务及开发计划完成状态，留存签收证据后复原当前问题单。 [项目交付流程](../../../../AGENTS.md#review-loop) · [归档收尾](#archive-closeout)

<a id="signed-native-verification"></a>
### 签收版原生验证

在 `v2.0` 基准 `cc0345a8439e0833bfbb4772f3da347084881507` 上逐项核对 35 个正式输入、6 个未跟踪源码路径、已跟踪差异及 206 个登记文件。实际指纹为 `88a646ea8ff63d547d5b58ac16cf2fbda7518aa1ce24f0857920c2c175d58419`，与 A 最终构建和 B 独立回放匹配。使用签收版产物复验 Debug、Release、有效开发源根和不可用根的四种产品启动，均退出 0。逐一重新校验 20 个产品包的消费者 ABI。 [签收源码与产物身份](../../../../build/d08-t07/final/signed-source.json) · [最终执行清单](../../../../build/d08-t07/final/verification-evidence.json) · [变更严格校验](../../../../build/d08-t07/final/validate-change.json)

核对五份增量规范的 9 项要求和 20 个场景。最终签收版的 Release 冷启动和 Debug/Release 双 View 复用 B 的实际回放。深度资源、CPU 包缓存与源位置、Ninja 配置选项等合同复用已登记的专门对照，验证范围如下。

| 要求 | 实现与验收事实 |
| --- | --- |
| 产品有限变体 | `ProductVariants` 统一打包与重载角色。`CheckProductProgram` 在 glslang 前校验组合和阶段，实际拒绝合法 JSON 中的错配值并定位字段。曝光变化继续复用同代同键。 [目录对照](../../../../build/d08-t07/b-recheck-2/Boundaries-Release.log#L8) · [曝光对照](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L17) |
| CPU Shader 包复用 | `ReloadCompiler::Acquire` 命中前核对输入，按条目和逻辑字节准入。真实替换 include 后三个依赖角色重建，其余六个复用，等内容保存命中九个包。两条目压力及单配置工具身份对照通过。 [CPU 回放](../../../../build/d08-t07/b-recheck-2/ReloadCpu-Release.log) · [Ninja 包身份](../../../../build/d08-t07/b-recheck-1/flag-check.log) |
| 设备级原生复用 | `ForwardDevice` 在 owner context 查找当前表和在途旧表。两路不同尺寸复用相容键，线性中间目标和 sRGB 终点各有独立键。冷单槽、满额撤销和真实退休均有链接计数与占用证据。 [原生键与额度](../../../../build/d08-t07/b-recheck-2/ReviewCorrections-Release.log#L2) · [恢复旧键](../../../../build/d08-t07/b-recheck-2/Boundaries-Release.log#L2) |
| 文件通知与输入 | `SourceReload::Poll` 合并源及接口目录通知，worker 复验实际依赖。Windows 替换保存触发目标重建，实际产品从打包路径和指定开发根都可启动。 [通知回放](../../../../build/d08-t07/b-recheck-2/ReloadCpu-Release.log#L3) · [启动清单](../../../../build/d08-t07/final/verification-evidence.json) |
| 有界候选准备 | `AdvanceFrameShaders` 接续当前修订，原生链接按每轮一个创建额度推进。最后一次 link 内的新保存使旧候选失去资格。冷设备分配失败保留原回执供恢复，更晚的修订接管待处理输入。 [修订及预算](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L26) · [冷启动交接](../../../../build/d08-t07/b-recheck-3/ColdDelivery-Release.log) |
| 整代发布与兼容性 | `ForwardRenderer::PlanView` 收集两路 Pipeline 键与显示输入，`PublishShaders` 整体切换，帧槽保有选定代。新增 FXAA、ABI 和覆盖纹理不匹配时可继续使用旧代。显示资源恢复和实际源码修复后正常提交并读回。 [双 View](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L20) · [深度图](../../../../build/d08-t07/b-recheck-2/ReviewCorrections-Release.log#L10) · [LUT 上传](../../../../build/d08-t07/b-recheck-2/Boundaries-Release.log#L7) |
| 旧代退休与关闭 | `ForwardFrame::Reset` 按安全槽归还旧代，原生包沿真实完成票据保活。受控 Pending 时旧输出保持，恢复完成后归还容量。退出先收拢源会话和 CPU 作业，再清理设备准备表。 [在途与退休](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L24) · [源会话关闭](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L30) |
| 构图冻结与 View 确认 | `ExecuteFrame` 对实际图出口校验冻结 Shader 代和 Graphics 票据。裁剪主路时离屏独立提交，安全 UI 保留原输入的输出和票据。 [安全 UI 与独立提交](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L15) |
| 只读统计 | `FrameStatistics::PublishShaders` 与接受场景快照分别发布，`DrawShaderStatistics` 读取存好的事实。实际面板观察期间原生工作不增长。缺字段预检的 GLSL 阶段增量为 0，两就绪显示阶段增加两次键命中。 [实际面板](../../../../build/d08-t07/b-recheck-3/RuntimeReload-Release.log#L20) · [计量对照](../../../../build/d08-t07/b-recheck-2/ReviewCorrections-Release.log#L8) |

Windows x64 上的签收版原生回放使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。设备分配失败、链接拒绝及旧帧 Pending 由探针触发，编译、GL link、提交和读回采用实际实现。设备身份和小额缓存的对照属于会话隔离检查，跨设备性能与阶段成本由 D08-T08 验收。

<a id="review-closeout"></a>
### 独立审查留存

B 确认 CR-001 至 CR-013 全部关闭，代码与注释可读性通过。注释覆盖 Shader 目录、编译和重载、Forward 原生表与显示准备、Runtime 双 View 提交、Editor 快照及实际构建输入。三次独立复核保留了逐项结论、运行边界和最终版本。 [第一次复核](#b-recheck-1) · [第二次复核](#b-recheck-2) · [最终签收](#b-recheck-3) · [B 签收问题单原件](../../../../build/d08-t07/final/review-signed.md)

<a id="spec-sync"></a>
### 主规范同步

创建 `shader-generation-reload` 主规范，将其它四份增量追加到对应能力。共合入 9 项要求和 20 个场景，逐项核对其正文与签收增量。受影响的四份现有主规范保留原有目的、顺序及全部 37 项要求和 109 个场景。OpenSpec 1.13.2 严格校验 40 份主规范通过。 [同步清单](../../../../build/d08-t07/final/spec-sync.json) · [主规范严格校验](../../../../build/d08-t07/final/validate-specs.json)

<a id="archive-closeout"></a>
### 归档收尾

OpenSpec 1.13.2 完成归档，20 项任务全部完成。归档后的活跃 change 列表为空，40 份主规范再次通过严格校验。核对 9 个归档工件及两份开发计划的引用，五份增量规范逐字节保留。开发计划的当前工作包已更新为 D08-T08。B 签收原件已留存，当前问题单已按模板复原。 [归档回执](../../../../build/d08-t07/final/archive.json) · [归档后校验](../../../../build/d08-t07/final/post-archive-validation.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [签收原件](../../../../build/d08-t07/final/review-signed.md)
