# Tasks

## 1. 编译输入和内容依赖

- [x] 1.1 在现有 `PhysaraRenderGraph` 目标接入纯 CPU 编译入口。以同一张图在封存前后验证阶段检查、计划与图代的关联及失败无部分结果。临时消费者通过公开入口链接图、Core 和公共 RHI。 [编译边界](specs/render-graph-compilation/spec.md#requirement-compile-a-complete-cpu-only-execution-plan)
- [x] 1.2 从读取、Load 和导出求实际范围的内容生产者。用纹理相邻 mip 和缓冲相邻字节各验证合并两个生产者，区间外更新无内容边。用未写或丢弃空洞验证拒绝，补足缺失范围后验证恢复。已有声明期结果仅在输入和证据匹配时复用。 [内容依赖](specs/render-graph-compilation/spec.md#requirement-resolve-dependencies-from-the-content-of-exact-resource-versions)

## 2. 存储顺序、活性和输出交接

- [x] 2.1 建立重叠 WAR/WAW 约束和非法 feedback/环诊断。用声明较晚的旧版读反转可运行顺序，再让该读依赖覆写者的另一结果，核对编译拒绝。用同范围采样加可写附件验证拒绝，并核对合法 Load、支持的只读深度联合采样及不相交范围。 [存储与反馈](specs/render-graph-compilation/spec.md#requirement-preserve-overlapping-storage-order-and-reject-illegal-feedback)
- [x] 2.2 发布与 `CopySource` 关联的类型化读回副作用。从读回及导出沿内容边裁剪，并以声明索引打破同等依赖的并列。用无图内产物的读回、无匹配读取的标记、失去消费者的上传及双分支重复编译验证结果。用两端存活、中间被裁剪且首写被旧版读推迟的三写图，核对 WAW 仍约束首尾。 [活性与顺序](specs/render-graph-compilation/spec.md#requirement-cull-work-from-explicit-output-and-side-effect-roots)
- [x] 2.3 生成外部交接记录。用同一旧版输出分别验证较新写入被裁剪、写入范围不相交、重叠写入因另一结果存活三种情况。再用无图内 Pass 的有效直接导入导出核对图尾位置和进出状态。 [输出合同](specs/render-graph-compilation/spec.md#requirement-keep-exported-ranges-available-at-the-end-of-the-plan)

## 3. 物理分配和执行所需的计划值

- [x] 3.1 按最终顺序计算首末使用、完整描述兼容的物理槽和瞬态字节预算。用相邻寿命、同一 Pass 重叠、导出延寿及同容量不同描述核对复用边界。以纹理完整 mip/层与缓冲容量的预期字节数核对计划总量，验证刚好足够、一字节不足及估算不可表示时的结果。 [物理计划](specs/render-graph-compilation/spec.md#requirement-plan-compatible-physical-storage-within-a-transient-budget)
- [x] 3.2 核对描述、给定能力、格式、样本和附件视图，并输出按 Pass 的视图、清除与实际用途计划。用给定能力拒绝一种已声明的用途，用同一 Pass 的尺寸或样本失配验证附件诊断。核对清除的传输用途先于后续附件访问，联合只读用途和范围仍可供执行器解析。 [执行计划](specs/render-graph-compilation/spec.md#requirement-compile-a-complete-cpu-only-execution-plan)

## 4. CPU 消费与 A 本地验证

- [x] 4.1 让同一独立消费者编译诊断图及字体上传、VB/IB/常量、`PresentColor` 清除、UI 合成及最终输出。对照预期的上传生产者、Load、图尾版本、进出状态与物理映射。成功和失败路径均观察回调计数为零，消费者仅使用 CPU 能力及资源替身。
- [x] 4.2 用普通 CMake 构建受影响的图目标及消费者，运行 Debug 和 Release 的合同检查，核对图目标的链接边界与正式构建入口。将实际命令、配置、输入与产物身份、结果和覆盖限制留存到本任务。已有匹配的 D05-T01 行为证据可复用，声明发布或范围语义变化时复验直接受影响用例。 [A 本地验证](#a-local-verification)

使用一套归类到 `build/d05-t02/` 的 CPU 消费者完成各组的直接检查，正式构建入口只使用仓库输入。对每个新增用例记明它能捕获的具体缺陷。签收输入和产物匹配且六条需求已有证据后停止扩展。原生物化、同步、GPU 像素与窗口验收沿 D05-T03 至 T05 推进。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

## 5. 独立 A/B 审查与注释

- [x] 5.1 A 本地验证通过后，在 [当前问题单](../../../../build/d05-t02/final-verification/b-signed-ledger.md) 登记基准、实际差异及新增文件、验证入口、产物身份和限制，由维护者调度 B。以问题单的可复现交接为完成依据。
- [x] 5.2 B 在独立会话核对实际差异、图编译与直接消费者证据，补齐涉及的完整第一方文件注释。用稳定编号记录发现和验证要求，并在问题单写明覆盖与结论。 [首轮发现 CR-001 至 CR-006](../../../../build/d05-t02/final-verification/b-signed-ledger.md#问题与整改)
- [x] 5.3 A 按编号整改并复跑区分缺陷的最小检查，记录修正和结果。如无正式发现，记录适用的自检结论。 [A 整改与自检](#a-correction-verification)
- [x] 5.4 B 独立核对修正版输入、回归和注释。关闭或有依据地撤回正式发现，分别记录代码正确性和注释可读性，再签收最终版本。 [B 独立复核与签收](#b-independent-recheck)

## 6. 最终验证与归档

- [x] 6.1 B 签收后按 OpenSpec verify 将需求和场景逐项对应到签收源码、CPU 消费者和结果。匹配的检查证据可复用，具体缺口按影响补验，并留存最终验证结果与适用范围。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步 `render-graph-compilation` 的 Purpose 与需求，运行锁定版本的 OpenSpec 严格校验，核对既有主规范及本次场景。 [主规范同步](#spec-sync)
- [x] 6.3 留存 A/B 与最终验证证据后归档。核对产物、链接和任务状态，更新开发计划中的 D05-T02 与下一工作包引用，归档成功后按模板重置当前问题单。 [归档结果](#archive-result)

<a id="a-local-verification"></a>
## A 本地验证

基准为 `v2.0` 的 `d5ee48e0e4adcccdc80a6cdcf324089fb457303b`，暂存区为空。编译消费原图的封存记录，向执行方交付同图代的只读计划。用调用方提供的能力表核对存储、边界状态、视图和用途。图内读回通过 `MarkReadback` 关联已声明的 `CopySource`，与候选 Pass 一起发布。 [编译入口](../../../../Engine/Renderer/RenderGraph/GraphCompiler.hpp) · [当前问题单](../../../../build/d05-t02/final-verification/b-signed-ledger.md)

从仓库根目录运行本地脚本。正式目标及独立 CPU 消费者用 Visual Studio 2022 x64、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 配置。脚本在两种配置各重建 `PhysaraRenderGraph` 和 `PhysaraGraphPlanChecks`，成功后运行对应的可执行文件。核对了 160 项构建及夹具输入、配置和 6 个产物的身份。独立消费者的 Physara 链接依赖为 RenderGraph、Core 和通用 RHI，Release PE 导入为 C/C++ 运行库和 Kernel32。 [运行脚本](../../../../build/d05-t02/a-local/VerifyGraphPlan.ps1) · [命令和退出码](../../../../build/d05-t02/a-local/handoff/commands.json) · [构建输入](../../../../build/d05-t02/a-local/handoff/source-sha256.json) · [配置](../../../../build/d05-t02/a-local/handoff/configuration-sha256.json) · [产物](../../../../build/d05-t02/a-local/handoff/product-sha256.json) · [链接依赖](../../../../build/d05-t02/a-local/handoff/project-link.log) · [PE 导入](../../../../build/d05-t02/a-local/handoff/cpu-dependencies.log)

```powershell
& 'build/d05-t02/a-local/VerifyGraphPlan.ps1' -EvidenceName 'handoff'
```

| 范围 | 能区分的回归与观察结果 |
|---|---|
| 1.1–1.2 | 编译拒绝未封存输入并保留原图。相邻 mip、相邻字节及分离的深度/模板来源均保留，范围外写入裁剪。未写和丢弃空洞拒绝，补写后可导出。 |
| 2.1–2.3 | 晚声明的旧读排在覆写之前。覆盖依赖成环和同范围读写反馈均拒绝，互不相交范围及给定能力支持的只读深度联合用途通过。读回根保留关联的 CopySource，错误标记和失败 setup 无部分发布。中间写被裁剪后首尾 WAW 仍成立。旧版导出仅在存活覆写与其范围相交时失败，直接导入保留图尾身份和边界状态。 |
| 3.1–3.2 | 同描述相邻寿命共享槽，重叠、导出延寿和不同用途描述分槽。按整份存储累计预算，验证块格式逐 mip 向上取整、恰好够用、一字节不足及不可表示。附件按所选 mip 尺寸核对，传输清除先于附件用途，缺失清除、样本或联合读能力时明确拒绝。 |
| 4.1–4.2 | 基线 UI 计划保留诊断图和字体上传、VB/IB 区间、完整对齐的常量视图、清除及合成 Load，图尾交接合成版本与宿主进出状态。成功和失败编译均未执行 Pass 回调。Debug、Release 各四组检查退出 0。 |

范围回溯、声明发布及导入状态相关的 T01 回归在本轮夹具中复验。工作树缺少 T01 历史夹具，未将其归档证据作为本轮实跑结果。阶段 Debug 编译曾在 MSVC PDB 处理处内部失败，退出码为 -1073741819。同源码和编译选项串行重试及本轮最终两配置构建均通过。原生物化、实际深度采样、同步、像素和窗口呈现由 D05 后续工作包验收。OpenSpec 1.13.2 严格结构校验 20/20 通过。 [检查源码](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp) · [Debug 结果](../../../../build/d05-t02/a-local/handoff/run-debug.log) · [Release 结果](../../../../build/d05-t02/a-local/handoff/run-release.log) · [MSVC 原日志](../../../../build/d05-t02/a-local/build-consumer-debug-ordering.log) · [OpenSpec 校验](../../../../build/d05-t02/a-local/openspec-strict.json)

<a id="a-correction-verification"></a>
## A 整改与自检

2026-11-14 以 `v2.0` 的 `d5ee48e0e4adcccdc80a6cdcf324089fb457303b` 上含 B 注释的版本整改。修改前核对 17 项 B 复现输入和两个原可执行文件，SHA-256 均匹配，暂存区为空。 [首轮问题单](../../../../build/d05-t02/final-verification/b-signed-ledger.md#问题与整改) · [B 输入核对](../../../../build/d05-t02/a-correction/b-input-match.json) · [B 到 A 的源码差异](../../../../build/d05-t02/a-correction/source-corrections.diff) · [整改版输入](../../../../build/d05-t02/a-correction/review-inputs.json)

| 条目 | 整改及本地复验结果 |
|---|---|
| CR-001 | 普通阶段按纹理 mip/layer 或整 Buffer 核对共享状态单元。两种复制源加绘制读取均拒绝且诊断指出双方用途，跨 mip、不同 Buffer 和兼容的顶点/索引读取通过。独立 typed clear 与随后采样保留分阶段计划。 |
| CR-002 | packed depth/stencil 的普通复制源、目标及外部读回须覆盖两种 aspect。三种单 aspect 传输请求分别被拒绝，完整 packed 传输及独立 typed clear 对照通过，回溯能区分各自的内容来源。 |
| CR-003 | `Presentation` 核对宿主要求的结束状态。`CopyDest` 与 `ShaderResource` 要求不兼容时，诊断含资源版本、实际值和要求值，正确结束状态通过。 |
| CR-004 | `ShaderRead` 纹理导出按实际采样视图核对范围。无视图和超出所支持层数时拒绝，匹配层数的视图及完整传输导出通过。 |
| CR-005 | 在名称前保留类型、资源索引及已知的 Pass 索引。不同索引的无名纹理得到不同正文，两个输入均因非法形状被拒绝。 |
| CR-006 | 同一 packed mip/layer 的深度采样与模板附件写被判定为非法反馈。跨 mip 和受支持的只读深度联合采样通过，颜色反馈对照仍拒绝。 |

以 MSVC 19.44.35228 x64 和 Windows SDK 10.0.26100.0 重建 Debug、Release。A 既有 `PhysaraGraphPlanChecks` 复跑结果为两配置各四组通过。脚本记录 160 项构建与夹具输入、配置及六个产物。补充覆盖单 aspect CopyDestination 与图尾读回、独立清除与普通读取，以及头尾导出的视图层数。 [正式图目标和 A 消费者命令](../../../../build/d05-t02/a-local/correction-handoff/commands.json) · [匹配源码](../../../../build/d05-t02/a-local/correction-handoff/source-sha256.json) · [匹配产物](../../../../build/d05-t02/a-local/correction-handoff/product-sha256.json) · [Debug](../../../../build/d05-t02/a-local/correction-handoff/run-debug.log) · [Release](../../../../build/d05-t02/a-local/correction-handoff/run-release.log)

B 复现副本沿用 Ninja Multi-Config，Debug、Release 退出码均为 0，每次核对 15 条预期输出。副本将 CR-006 的 packed 读回对照改为明确读回两个 aspect，以便单独验证反馈。B 原夹具、产物和日志保留。A 既有分 aspect 内容检查使用 typed clear。原生物化、深度采样、同步及窗口交接仍由后续工作包验证。 [夹具差异](../../../../build/d05-t02/a-correction/fixture-corrections.diff) · [Ninja 复跑入口](../../../../build/d05-t02/a-correction/VerifyReviewCases.py) · [命令](../../../../build/d05-t02/a-correction/correction-review/commands.json) · [Debug](../../../../build/d05-t02/a-correction/correction-review/run-debug.log) · [Release](../../../../build/d05-t02/a-correction/correction-review/run-release.log) · [OpenSpec 严格校验](../../../../build/d05-t02/a-correction/openspec-strict.json)

<a id="b-independent-recheck"></a>
## B 独立复核与签收

基准为 `v2.0` 的 `d5ee48e0e4adcccdc80a6cdcf324089fb457303b`，暂存区为空。复核前 21 个变更文件和 29 份证据与 A 整改交接的 SHA-256 一致，含新增图源码。B 签收清单包含补齐注释后的图模块输入、change 合同和验证证据身份，SHA-256 为 `539155a48a39288d10b2054747c2a4757910b8368345a98809ddc57dd88dcaba`。 [A 整改交接](../../../../build/d05-t02/a-correction/review-inputs.json) · [B 签收清单](../../../../build/d05-t02/b-recheck/accepted-inputs.json)

| 条目 | B 复核结果 |
|---|---|
| CR-001 | 共享 mip/layer 的复制源与采样、同 Buffer 的复制源与顶点读取均被拒绝。跨 mip、分离 Buffer、联合只读和分阶段清除对照通过。补充检查确认同 Buffer 的非重叠复制保留两个状态请求和原区间。 |
| CR-002 | packed 格式的单 aspect 复制源、复制目标与图尾读回均被拒绝。完整 packed 复制及分 aspect typed clear 通过，内容来源可分别回溯。 |
| CR-003 | 呈现导出按宿主完整 final 状态校验。错误出口保留实际值、要求值与资源版本，`ShaderResource` 对照及基线 UI 计划通过。 |
| CR-004 | 纹理 `ShaderRead` 导出与 Pass 采样共用同格式视图能力检查。无可用视图或超出层数上限时拒绝，能力匹配的范围及合法传输导出通过。 |
| CR-005 | 无名资源的诊断可按索引定位。补充检查确认长名称截断后仍保留冲突原因、资源索引、Pass 索引和版本。 |
| CR-006 | 同一 packed mip/layer 的采样与可写附件被拒绝，跨 mip 对照通过。只读深度联合采样按给定能力接受或拒绝，精确 aspect 仍用于内容回溯。 |

**代码正确性** B 独立复核通过，关闭 CR-001 至 CR-006。核对 A 的 160 项构建和夹具输入、3 项配置、6 个产物，以及复现副本的 21 项输入和两个可执行文件。B 复跑两套匹配的 Debug、Release 程序，四次均退出 0。B 用 MSVC 19.44.35228 x64、Windows SDK 10.0.26100.0 和 Ninja Multi-Config 重建独立消费者，复验原问题和补充对照，两配置均构建和运行成功。 [匹配产物复跑](../../../../build/d05-t02/b-recheck/runs.json) · [独立构建与运行](../../../../build/d05-t02/b-recheck/builds-and-runs.json) · [Debug](../../../../build/d05-t02/b-recheck/run-debug.log) · [Release](../../../../build/d05-t02/b-recheck/run-release.log)

**注释可读性** B 复核通过。复查图模块的 11 个 C++ 文件和 CMake 入口，补充六个 C++ 文件中的状态粒度、传输范围、采样视图、呈现结束状态与诊断寿命说明。测试后只改整行注释，已核对非注释行和预处理语义。CPU 编译检查使用显式能力夹具，原生物化、同步、像素和窗口交接由 D05-T03 至 T05 验证。 [当前问题单](../../../../build/d05-t02/final-verification/b-signed-ledger.md#b-的注释覆盖)

问题单记录的注释覆盖包括正式 CMake 目标的 CPU 链接边界，`RenderGraph` 与 `PassResources` 的候选发布、借用寿命和读回标记，资源类型与范围辅助代码的版本回溯、传输和清除合同，编译入口及内部依赖的阶段和诊断语义，以及用途和存储计划的状态粒度、能力、图尾交接与预算。B 在这些条目均记录无剩余缺口。 [B 注释覆盖原文](../../../../build/d05-t02/final-verification/b-signed-ledger.md#b-的注释覆盖)

<a id="final-verification"></a>
## 最终验证

2026-11-14 按 OpenSpec 1.13.2 的 spec-driven 工作流核对四份规划产物和十三项实施及独立审查任务。基准为 `v2.0` 的 `d5ee48e0e4adcccdc80a6cdcf324089fb457303b`，暂存区为空。12 个图模块构建和源码文件、3 份 change 合同及 3 份证据清单与 B 签收一致。签收问题单原件的 SHA-256 为 `5b4d1555b33074278518243aaaa2da07152186ed9c237cd5d9010cfd9cd32433`，其中相对链接以原 `Docs/` 位置为基准。 [B 独立复核与签收](#b-independent-recheck) · [签收问题单原件](../../../../build/d05-t02/final-verification/b-signed-ledger.md)

用 Visual Studio 2022 x64、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0，在独立构建目录重建正式 `PhysaraRenderGraph` 目标及既有 `PhysaraGraphPlanChecks`。Debug 和 Release 各四组检查通过，退出码均为 0，160 项构建和夹具输入的 SHA-256 构建前后一致，并记录了三项配置和六个产物。消费项目链接 RenderGraph、Core 与公共 RHI，Release 程序只导入 C/C++ 运行库和 Kernel32。首次配置遇到 SDK 目录访问拒绝（MSB4184），获准访问后以相同配置重试通过。 [运行脚本](../../../../build/d05-t02/a-local/VerifyAcceptedGraphPlan.ps1) · [命令和退出码](../../../../build/d05-t02/final-verification/commands.json) · [构建输入](../../../../build/d05-t02/final-verification/source-sha256.json) · [配置](../../../../build/d05-t02/final-verification/configuration-sha256.json) · [产物](../../../../build/d05-t02/final-verification/product-sha256.json) · [首次配置日志](../../../../build/d05-t02/final-verification/attempt-msbuild-sdk-denied/configure-formal.log) · [链接依赖](../../../../build/d05-t02/final-verification/project-link.log) · [PE 导入](../../../../build/d05-t02/final-verification/cpu-dependencies.log) · [Debug](../../../../build/d05-t02/final-verification/run-debug.log) · [Release](../../../../build/d05-t02/final-verification/run-release.log)

复用 B 已通过的四次匹配产物复跑和两次独立构建运行，独立回归每种配置核对 17 条输出。核对 B 的 163 项构建输入、可执行文件和运行日志的 SHA-256。六处签收前后的源文件差异与 B 保留的注释快照一致，最终重建使用了含注释的签收版本。 [B 证据核对](../../../../build/d05-t02/final-verification/b-evidence-match.json)

| 需求 | 场景和签收证据 |
|---|---|
| [精确版本和内容来源](specs/render-graph-compilation/spec.md#requirement-resolve-dependencies-from-the-content-of-exact-resource-versions) | `TraceContent` 按内容效果从请求版本回溯。双生产者场景检查纹理相邻 mip 和缓冲相邻字节，范围外写入被裁剪。未写及丢弃场景检查未定义诊断、补齐后恢复和分 aspect 内容，传输范围另由 CR-002 复核。 [范围回溯](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L142) · [双生产者](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L190) · [空洞和恢复](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L240) · [packed 传输](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L307) |
| [存储顺序和反馈](specs/render-graph-compilation/spec.md#requirement-preserve-overlapping-storage-order-and-reject-illegal-feedback) | `BuildUseEdges` 为重叠访问建立 WAR/WAW，编译在裁剪前拒绝环。四个场景覆盖旧版读先于覆写、裁剪中间写入后首尾保持顺序、环诊断、合法 Load 和受支持的只读深度采样与可写附件反馈的区分。CR-001、CR-006 验证普通阶段共用状态单元的限制。 [存储边](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L195) · [状态组合](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp#L313) · [旧版读及环](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L379) · [存活 WAW](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L539) · [附件反馈](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L421) · [编辑器 Load](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L829) |
| [活性和稳定顺序](specs/render-graph-compilation/spec.md#requirement-cull-work-from-explicit-output-and-side-effect-roots) | `MarkReadbackUse` 关联已声明的 `CopySource`，`MarkLive` 仅沿 RAW 保留生产者，`SortPasses` 从当前可运行的 Pass 中选择最小声明索引。两个场景覆盖存活读回、裁剪被覆写上传和重复编译得到相同顺序及物理槽。 [读回标记](../../../../Engine/Renderer/RenderGraph/PassResources.cpp#L258) · [活性](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L403) · [确定排序](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L335) · [读回与稳定分支](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L479) |
| [图尾导出](specs/render-graph-compilation/spec.md#requirement-keep-exported-ranges-available-at-the-end-of-the-plan) | `CheckExport` 检查活性写入与所请求版本范围是否冲突。旧版场景分别覆盖死写、存活非重叠写和存活覆写。直接导入导出保留外部身份和进出状态，CR-003、CR-004 分别验证呈现结束状态和采样视图。 [覆盖校验](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L420) · [导出用途](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp#L455) · [旧版导出](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L572) · [直接导入](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L617) · [导出视图](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L640) |
| [寿命、复用与预算](specs/render-graph-compilation/spec.md#requirement-plan-compatible-physical-storage-within-a-transient-budget) | `BuildStoragePlan` 按最终顺序和图尾计算首末使用，`AssignSlot` 计入兼容槽的完整存储字节。复用场景覆盖寿命分离、同时使用、导出延寿和不同描述。预算场景覆盖刚好足够、少一字节和不可表示的估算。 [存储计划](../../../../Engine/Renderer/RenderGraph/GraphPlanStorage.cpp#L169) · [槽与预算](../../../../Engine/Renderer/RenderGraph/GraphPlanStorage.cpp#L138) · [寿命和预算边界](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L662) · [计量与溢出](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L733) |
| [完整 CPU 计划](specs/render-graph-compilation/spec.md#requirement-compile-a-complete-cpu-only-execution-plan) | `CompileRenderGraph` 消费封存图和显式能力，只交付完整计划或诊断。编辑器场景检查上传、采样、缓冲读取、清除、Load、导出及成功和失败时回调次数均为零。能力场景检查不支持的清除或样本及附件所选 mip 尺寸失配。CR-005 验证有界诊断保留定位信息。 [编译入口](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp#L511) · [用途计划](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp#L539) · [封存阶段](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L174) · [编辑器计划](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L829) · [能力和附件](../../../../build/d05-t02/consumer/GraphPlanChecks.cpp#L776) |

六条需求及十三个场景均有源码和检查对应。图代、范围、存活顺序、逻辑预算和纯 CPU 计划边界符合 design 和架构 §6.3，OpenSpec 同步前严格校验 20/20 通过。原生物化、同步、GPU 像素和窗口交接由 D05-T03 至 T05 验收。 [设计决定](design.md#decisions) · [架构 §6.3](../../../../Docs/Architecture.md#graph-compile) · [同步前严格校验](../../../../build/d05-t02/final-verification/openspec-strict-before-sync.json)

<a id="spec-sync"></a>
### 主规范同步

按 OpenSpec 1.13.2 同步新增能力 `render-graph-compilation`。主规范保留 delta 的 Purpose、六条需求和十三个场景，既有 19 份主规范的 SHA-256 均匹配。20 份主规范全部通过严格校验，含活动 change 共 21/21 通过。 [主规范](../../../specs/render-graph-compilation/spec.md) · [同步清单](../../../../build/d05-t02/final-verification/spec-sync.json) · [同步后严格校验](../../../../build/d05-t02/final-verification/openspec-strict-after-sync.json)

<a id="archive-result"></a>
### 归档结果

`openspec archive compile-render-graph-plan --yes --skip-specs --json` 退出 0，归档目录为 `2026-11-14-compile-render-graph-plan`。移动前已同步并复核主规范。移动时 15/16，剩余 6.3 为归档收尾。五份产物的 SHA-256 在移动前后均匹配，随后按归档路径调整相对链接并保留历史问题单引用。活动 change 清单为空，归档后 OpenSpec 严格校验 20/20 通过。 [CLI 记录](../../../../build/d05-t02/final-verification/archive-cli.json) · [移动前清单](../../../../build/d05-t02/final-verification/archive-inputs.json) · [归档后严格校验](../../../../build/d05-t02/final-verification/openspec-strict-after-archive.json)

开发计划记录 D05-T02 已验收归档，D05-T03 待立项。归档、链接和签收身份复核通过后，当前问题单按空模板重置，SHA-256 与模板一致。任务完成 16/16。 [重置前链接和身份核对](../../../../build/d05-t02/final-verification/archive-check-before-reset.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [问题单模板](../../../../Docs/CodeReview.md.tpl) · [重置记录](../../../../build/d05-t02/final-verification/ledger-reset.json)
