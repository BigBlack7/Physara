# Tasks

## 1. View 物理请求与相机准备

- [x] 1.1 A 将七项有单位的物理参数加入主 View 请求，建立统一缺省与共享求值。通过合法区间和派生结果校验提交 `EV100` 及 FP32 `preExposure`。用 V1 的曝光解析锚点、零输入及有限但超出倍率表示范围的请求核对数学和失败后状态。
- [x] 1.2 A 在 `ViewWork::Prepare` 接入导航镜头、场景透视有效焦距和正交分支，填入本轮相机及光学/曝光快照。验证 V1 的 60° 缺省、单位换算、导航 FOV 变化和切换实体时源投影的权威性。补齐请求与快照的单位、版本和用途说明。

## 2. 帧内 ABI 与接受回执

- [x] 2.1 A 在权威 `StaticForward` View schema 增加预曝光字段，更新生成布局、`PrepareViewRow` 编码和行复用比较，沿现有图内上传交付。用 V2 检查正式包、反射及真实 GL 读回，确认同一复用槽在矩阵和模式相同时取得新的倍率。
- [x] 2.2 A 将与物理请求匹配的相机快照沿现有场景候选和正常图接受记录发布。用 V3 检查首次待确认、陈旧候选、等待或安全回退，以及实际采样并接受的新一帧。验证当前请求与最后渲染记录各自对应真实版本。

## 3. v1 持久化与工作区编辑

- [x] 3.1 A 扩展场景文件边界的 `mainView.requested.physicalCamera` 读写，复用 View 校验及同一缺省。用 V4 核对缺少物理对象的原 v1、七个非默认字段的完整保存重开，以及显式坏对象失败时的活动状态。
- [x] 3.2 A 在 View Settings 接入有单位的物理参数和有效镜头读回，显示当前 EV100/倍率和最后成功值，通过现有 View 会话事务提交。以 V4 的正式入口核对合法编辑、Undo/Redo、拒绝和文档冻结。同步实际使用说明与 v1 字段兼容说明。 [构建与运行](../../../../Docs/Build.md)

## 4. A 本地验证与交接

- [x] 4.1 A 核对 D07-T02 的三个开发目标、V1–V4 对应的源码和真实消费者、OpenSpec 严格校验及链接。记录命令、输入、包、exe、设备和未覆盖范围。登记实际待审 diff、未跟踪文件及验证入口，交由维护者调度 B。 [当前问题单](../../../../Docs/CodeReview.md)

## 5. 独立审查、整改和复核

- [x] 5.1 B 独立核对当前变更、直接消费者、物理求值、ABI、版本确认及场景兼容。通读涉改完整第一方文件并补齐职责、单位、入口约束和关键流程注释，登记实际覆盖、稳定 CR 编号及结论。 [审查闭环](../../../../AGENTS.md#review-loop) · [B 审查与最终复核](#b-final-review)
- [x] 5.2 A 按稳定 CR 编号整改并自检，将修正、受影响的回归结果及输入身份登记。无待整改项时据实登记。 [A 接收记录](#a-review-intake)
- [x] 5.3 B 复核最终实际输入、整改证据及涉改注释，分别确认功能和注释质量，登记签收结果。 [B 最终复核](#b-final-review)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后运行或复用匹配的 V1–V4，完成最终 verify。核对实际源码、生成包、原生提交及会话结果，记录证据与范围。 [最终验证](#final-verification)
- [x] 6.2 A 在本任务表留存审查、整改、签收和验收结果，同步四个能力的规范增量，核对保留的原有行为和新增场景，运行严格校验。 [规范同步](#spec-sync)
- [x] 6.3 A 通过 OpenSpec 归档，核对归档链接和开发计划完成状态。保留完整证据后按模板恢复当前问题单。

<a id="verification"></a>
## 验证范围与停止条件

开发验证材料放在 `build/d07-t02/`。优先复用 D06 的相机与文档验证方法、D07-T01 的正式 Forward 原生读回链。复用前核对当前源文件、生成 ABI、包、消费者和输入身份。受影响的正式包使用当前构建生成。 [既有 View 验收](../2026-11-16-prepare-static-scene-views/tasks.md#final-verification) · [既有场景文档验收](../2026-11-16-add-static-scene-documents/tasks.md#final-verification) · [前置原生验收](../2026-11-17-add-core-surface-brdf/tasks.md#final-verification) · [验证范围规则](../../../../.agents/skills/physara-verification/SKILL.md)

| 检查 | 最小有效输入与独立预期 | 停止条件 |
|---|---|---|
| V1 · 物理数值和投影 | 用 f/2、0.25 秒、ISO 200 得到 EV100=3 和倍率 0.10416667。补偿 +1 档时倍率约 0.20833333，将基准请求的快门减半时倍率约 0.05208333。导航缺省为 60°，24 mm 传感器配 12 mm 焦距时为 90°。同一 View 切到 60° 场景透视时，有效显示焦距约为 20.78460969 mm，保留源 aspect 和 near/far；切到正交时保持同一曝光请求与原正交投影。以零 ISO 与有限但超出倍率上界或下界的补偿区分原始字段与派生边界拒绝，定向检查 Release 的合法拒绝路径。 | 解析锚点、逆向焦距、投影来源及单位一致，值域失败可定位且无部分请求生效。曝光单独变化时几何矩阵、裁剪输入及布局匹配。 |
| V2 · 生成 ABI 与原生常量 | 构建正式 Debug `Physara`、`StaticForward` 和 `StaticDepth`，核对生成字段及实际反射。以 build 内开发包从相同生成 `ForwardView` 读出 `preExposure`，沿正式 Forward 准备、上传、绑定和绘制提交，复用同一已完成槽。两帧采用 V1 的 0 和 +1 档，矩阵、布局和模式固定。读回线性浮点附件并按实际格式设容差。 | 实际 GLSL 取得两次不同且成 2 倍的倍率，第二次来自当前行，受影响的正式消费者使用对应 ABI。用新包定向核对既有 Forward 预览和共用深度覆盖。 |
| V3 · 场景接受的曝光 | 用带实际图提交的场景先确认一个倍率，再改变曝光。用一份未获确认的候选证明等待、失败或安全回退保持先前记录，另用已准备但被新请求取代的候选检查过期结果。最后以匹配请求的实际场景结果、UI 采样和正常提交确认新值。 | 请求值先行变化，前一有效帧记录在失败期间可识别。首次确认前及仅有测试图时场景曝光保持待确认或原有效值。最后的相机、预曝光、Scene 和输出代对应同一次成功结果。 |
| V4 · 正式编辑与 v1 文件 | 用一个合法 v1 文件省略 `physicalCamera` 打开，核对原视场与 v1 缺省。经 View Settings 核对非默认焦距与曝光的提交、Undo/Redo 和 dirty。以七项均非默认的合法请求验证完整保存并重开。用同一文件的 `physicalCamera:null` 变体证明对象拒绝，执行一次零快门提交及打开期间的冻结检查。 | 恢复的七个字段及单位明确，用户能读回当前和最后成功值。坏文件和非法编辑保留原活动状态，完成保留此前已有合同的定向检查后停止。 |

记录实际运行与复用的证据。单台设备的原生结果按设备与驱动范围记录，T03–T05 进一步验证实际光照、环境和显示对预曝光的消费。

<a id="a-local-evidence"></a>
## A 本地验证记录

从仓库根目录执行 `& ./build/d07-t02/Verify.ps1`，以 MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 配置 Ninja Multi-Config。构建 Debug `Physara`、Release `PhysaraRenderer` 与当轮 CPU/原生探针。Debug 产品在独立工作目录运行 `Physara.exe --hidden --poll-limit=6`，退出码为 0。原生回放使用 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。 [回放入口](../../../../build/d07-t02/Verify.ps1) · [实际构建命令](../../../../build/d07-t02/a-local/product-build-commands.txt)

| 检查 | 本轮实测 |
|---|---|
| V1 | Debug 与 Release `NDEBUG` 均确认基准 EV100 为 3，当前倍率为 5/48，+1 档为 5/24，快门减半为 5/96。验证导航默认 60°、24 mm / 12 mm 导航镜头 90°、场景 `yfov` 权威性及正交保持。零 ISO、补偿溢出或下溢及无穷对焦距离均被拒绝，原请求完整保留。 |
| V2 | 分别核对正式 `StaticForward`、`StaticDepth` 和探针的生成接口、包反射与内容身份。`ForwardView` 为 80 字节，`preExposure` 偏移为 68。沿真实 Forward 行上传与 GL 提交，矩阵和模式保持相同，复用同一槽得到 `RGBA16_FLOAT` 读回 0.1041259766 和 0.2082519531。正式遮罩覆盖在两路深度配置下均读到同样的预览颜色与 D32 深度 0.75 / 0.25。 |
| V3 | 首次仅提交 UI 时没有已渲染曝光。修改请求后，无可准备视口及过期候选均保留已确认记录。随后正常图提交并经 UI 采样，第 8 个提交发布与活动 Scene 及请求版本一致的倍率 0.2083333284。 |
| V4 | 通过正式 View Settings 核对焦距、曝光补偿、同值 Enter、零快门拒绝、dirty、Undo/Redo 和 90° 镜头下的平移。旧 v1 缺项恢复默认，七个非默认值保存后重开一致。打开期间拒绝冻结文档的编辑，场景替换后旧 ISO 草稿无法写入新场景，`physicalCamera:null` 拒绝且保留原活动文档。 |

复用 D07-T01 的五个 glTF 验证输入与 Forward 读回方法。回放前后核对 1591 个正式输入与实际 exe、库、包、探针及场景输入身份，另外核对包登记的 23 项依赖。交接清单 SHA-256 为 `26ba7891687ed2a6218d086c334314c445094506a6a489876b77dbef1d203a2a`。原生提交和编辑回放覆盖上述单台设备的 Debug，Release 覆盖 CPU 数值和拒绝路径。T03–T05 负责后续实际照明、环境与显示消费验证。 [交接清单](../../../../build/d07-t02/a-local/handoff.json) · [原生回放日志](../../../../build/d07-t02/a-local/native.log) · [待审实现差异](../../../../build/d07-t02/a-local/a-source.diff)

`openspec validate add-physical-camera-exposure --type change --strict --json` 与 `git diff --check` 均通过。核对本 change、使用说明、当前阶段和问题单共 102 个本地链接及锚点，待审差异与回放证据摘要匹配。

<a id="a-review-intake"></a>
## A 审查接收记录

第 1 轮 B 审查覆盖 15 个第一方源码及接口文件，补充 14 个 C++ 文件的注释，登记正式 CR 0 项。A 接收该交付，功能整改不适用。 [第 1 轮问题单快照](../../../../build/d07-t02/final/review-signed.md)

在 `v2.0` 的基准 `126f636e887b307aa236f81c2343a35d222a373b` 上，用 A 的交接差异在 `build/d07-t02/` 下还原 B 接收源码。按问题单的 UTF-8/LF 算法重算 15 个文件的接收摘要 `aa5ac602cc275069850f75f90782d064420b13ddcb2d7b924536233eeedeb70e` 和交付摘要 `76d822c5305891bd6241a7e23aa8a2542068e6899c43753e7f2a07e90a48736d`，均匹配。逐文件核对原清单中的 1591 个正式输入，变化范围仅为这 14 个 C++ 文件。对比 49 处差异，去除整行注释、两处行尾 Doxygen 注释及空行后，代码行一致，预处理指令及续行一致。

本环节按 L0 核对注释与文档。`git diff --check`、OpenSpec 严格校验及本地链接与锚点检查均通过。V1–V4 原回放记录保留其实际输入身份。B 最终复核签收后，在 6.1 核对最终输入及原生验证记录。

<a id="b-final-review"></a>
## B 审查与最终复核记录

2026-11-17，B 完成初审与 A 接收后的最终复核。初审通读 14 个 C++ 文件和 `StaticForward.json`，按物理求值、相机准备、生成 ABI/行上传、正常图接受、编辑事务及 v1 持久化核对直接消费者；正式 CR 0 项，A 功能整改不适用。B 在 14 个 C++ 文件补充职责、单位、请求版本、失败状态及关键流程说明，核对去注释内容、预处理指令和续行，未改变行为。 [签收问题单与注释覆盖](../../../../build/d07-t02/final/review-signed.md)

最终复核直接从 `v2.0`、基准 `126f636e887b307aa236f81c2343a35d222a373b` 的当前工作区复算 15 文件摘要。按问题单定义的 UTF-8/LF 算法，结果为 `76d822c5305891bd6241a7e23aa8a2542068e6899c43753e7f2a07e90a48736d`，与上轮 B 注释交付完全一致，暂存区为空。本次没有新增源码或注释修正，A 接收与自检记录对应这一版本。B 重新核对设计及四份 spec、当前 diff、`git diff --check`、OpenSpec 严格结构校验及审查记录本地链接；未发现新增问题，代码审查和注释可读性均通过，B 签收该版本。

本次复用上轮完整源码审查；V1–V4 运行结果引用 A 的原记录，B 未复跑构建、GPU 读回或界面回放，未扫描 build 或独立核验其中的生成包/exe。6.1 继续由 A 核对最终输入与实际原生证据并完成最终 verify。本记录完成 5.1/5.3，不代表 6.1–6.3 已完成；签收后若改变代码或构建输入，重新交 B 复核受影响范围。

<a id="final-verification"></a>
## 最终验证与证据

2026-11-17，A 核对 B 签收的 15 个源码及接口文件，复算摘要 `76d822c5305891bd6241a7e23aa8a2542068e6899c43753e7f2a07e90a48736d` 与签收记录匹配。相对 A 初次交接，1591 项正式输入中仅 B 补充注释的 14 个 C++ 文件发生变化。签收问题单快照 SHA-256 为 `c3085c3f1625b9c8eb2d0c9d1ca0f8bc1d2286dc18509104dae2a97f1666a7db`。 [签收快照](../../../../build/d07-t02/final/review-signed.md) · [最终源码清单](../../../../build/d07-t02/source-manifest.json)

从仓库根目录再次执行 `& ./build/d07-t02/Verify.ps1`，以 Windows x64、MSVC 19.44.35228、Windows SDK 10.0.26100.0 和 Ninja Multi-Config 重建签收版 Debug `Physara`、Release `PhysaraRenderer` 及探针。Debug 产品执行 `Physara.exe --hidden --poll-limit=6`，退出码为 0。回放前后核对 1591 项正式输入、实际 exe、库、生成包和标样，另核对实际包的 23 项依赖。最终清单 SHA-256 为 `2bb6b845278f6bc78d0c535e1210c6d8da1a38e2e9c1557c7708e601b1dc268f`。 [回放入口](../../../../build/d07-t02/Verify.ps1) · [最终证据清单](../../../../build/d07-t02/handoff.json) · [实际产物与输入](../../../../build/d07-t02/run-inputs.json)

| 新增和修改的规范要求 | 签收版覆盖 |
|---|---|
| `view-preparation`，2 项要求、5 个场景 | V1 在 Debug 和 Release `NDEBUG` 核对 EV100=3、补偿与快门档位、60° 默认镜头、90° 导航镜头、场景透视与正交、非法值拒绝和曝光与几何独立。相机解析失败及父级尺度语义核对既有准备分支。V2 核对 `ForwardView` 80 字节、倍率偏移 68，正式颜色/深度包和探针均匹配同一 ABI，同槽 GL 读回为 `0.1041259766`、`0.2082519531`。 [物理求值](../../../../Engine/Renderer/View.cpp#L47) · [帧准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp#L305) · [常量编码](../../../../Engine/Renderer/Draw/StaticDraw.cpp#L354) |
| `editor-viewport-output`，1 项要求、2 个场景 | V3 核对首次 UI-only、等待及过期候选均保留对应的曝光记录。实际 Runtime 正常图经 UI 采样和提交接受后，以第 8 个提交确认 `0.2083333284`，对应相同 View 文档代及 Scene。 [发布检查](../../../../Engine/Renderer/View.cpp#L479) · [正常图确认](../../../../Runtime/Frame.cpp#L329) |
| `scene-document-persistence`，1 项要求、5 个场景 | V4 用正式会话验证旧 v1 缺省、七字段保存重开和 `null` 拒绝。字段缺失、未知字段和非法值核对同一格式及 View 预检入口，已有相机引用校验和比例待确认的保存语义沿用。 [读取](../../../../Editor/Session/SceneDocumentRead.cpp#L496) · [写入](../../../../Editor/Session/SceneDocumentWrite.cpp#L272) |
| `editor-domain-edits`，1 项要求、2 个场景 | V4 回放实际 View Settings 的焦距、曝光补偿、同值确认和非法快门，验证 Pan、Undo/Redo、dirty、Open 冻结与场景替换时的草稿隔离。 [面板](../../../../Editor/Panels/ScenePanels.cpp#L327) · [编辑事务](../../../../Editor/Session/EditorSession.cpp#L242) |

五项要求与 14 个场景均核对了实现入口。V1–V4 复验通过，保留既有投影、布局、正常图回执和会话事务路径。原生回放覆盖 Debug 下单台 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。Release 覆盖 V1 CPU 数值和拒绝路径。直接光、环境和显示对预曝光的实测按 D07 后续工作包进行。 [原生回放日志](../../../../build/d07-t02/native.log) · [Release CPU 日志](../../../../build/d07-t02/v1-Release.log)

<a id="spec-sync"></a>
## 规范同步

最终验证后同步四个能力，新增 3 项要求、更新 2 项要求。核对 14 个关联场景，其中 4 个为保留的相机准备与文档场景。四份主规范保留既有 Purpose 和其余要求。`openspec validate --specs --strict --json` 通过全部 31 份主规范。 [相机准备](../../../specs/view-preparation/spec.md) · [视口输出](../../../specs/editor-viewport-output/spec.md) · [场景持久化](../../../specs/scene-document-persistence/spec.md) · [编辑事务](../../../specs/editor-domain-edits/spec.md)

<a id="archive-closeout"></a>
## 归档收尾

`openspec archive add-physical-camera-exposure --skip-specs --yes --json` 已将 change 及 `.openspec.yaml` 移至 `2026-11-17-add-physical-camera-exposure`，四份主规范已在归档前同步。归档相对链接和开发计划登记已核对。D07-T02 验收完成后，阶段入口转到 D07-T03。签收和验证证据留存后，当前问题单按模板恢复。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [当前问题单](../../../../Docs/CodeReview.md)
