# Tasks

## 1. 场景层级与编辑事务

- [x] 1.1 接通 Hierarchy 与 Inspector 的只读领域数据。按活动 Scene 和版本刷新最小树投影，裁剪可见行，使用实体身份区分重名条目。用 V1 检查层级、选择、隐藏后的展示工作与文档替换失效。
- [x] 1.2 接通本地平移、旋转和有符号缩放的字段意图，沿现有 Scene 和会话历史发布离散编辑、预览、完成和取消。用 V1 核对缩放父级下的本地及世界结果、失败输入、一次手势的撤销重做及失效后不重放旧输入。
- [x] 1.3 接通当前选择的 Delete，沿子树事务维护选择、相机引用和重放。将文件、删除和现有菜单快捷键接入统一动作及启用规则。用 V1 核对删除重放、实体身份变化、文本及模态争用与按键 Repeat。

## 2. 文档操作与模型导入

- [x] 2.1 实现 File 的 New/Open/Save/Save As 和受控路径输入，显示操作、未保存状态、取消及失败。用 V2 核对首次及后续保存、Open 冻结和失败保留、取消后继续编辑，以及 JSON v1 的实际重新打开。
- [x] 2.2 接通保存后继续、明确放弃和取消。原替换意图绑定文档身份与版本，保存后继续核对准确的成功回执和保存后的当前版本。用 V2 区分准确保存、取消及写入期间接受的新编辑，确认只有匹配的成功可以继续替换。
- [x] 2.3 接通 Import Model 的路径、可选源 scene、进度和取消。会话等待匹配的 CPU 结果并通过一笔事务提交，显示资产支持说明与实际失败。用 V2 核对同源独立实例、导入中取消或文档替换、失败和失效请求不可采用旧版本。
- [x] 2.4 在 Runtime 继续的 CPU 轮次按资产发布、文件工作、实例化顺序推进，终止轮次退役导入并收拢文件及资产。用 V2 驱动暂停或槽不可用的代表性边界，核对等待活动编辑和无 GPU 依赖的提交、终止不发布。

## 3. View 面板、输入和当前帧请求

- [x] 3.1 使 View 面板显示相机源、导航操作、Forward 预览、请求比例、有效输出和原因。用 V3 核对实体相机选择、回到导航位姿及各 View 请求独立，V2 核对主 View 请求的保存与恢复。
- [x] 3.2 用当前封存布局和正常图的接受结果建立命中资格。保留单次布局像素请求和已有 UV 映射，用 V3 核对非 1:1 framebuffer 比例、非 1 render scale、留边及被裁剪区域，核对无匹配图像时的输入门控。
- [x] 3.3 实现捕获状态和 fly/orbit/pan，将相对光标请求交给真实宿主并从实际观察开始处理；将合法导航姿态接到单手势历史。用 V3 核对相机轴、orbit 距离、位姿重放、失焦/隐藏后的归还和新按下。
- [x] 3.4 让 Runtime 将 UI 最终事务、像素请求和相机交给既有 SceneFrame、Forward、UI 和呈现链；实际正常结果才更新导航命中依据。用 V4 核对视口隐藏、恢复和 canvas/camera 更新后的同一 token，以及编辑导致的真实输出变化。

## 4. 正式编辑器工作流

- [x] 4.1 更新 [构建与运行说明](../../../../Docs/Build.md) 的实际菜单、键鼠操作和相对资源根，核对说明、菜单名称、命令及链接可用。
- [x] 4.2 执行 V4，从正式编辑器的模型和文件动作开始，贯穿属性事务、场景准备、图上传和实际接受的窗口画面，再验证保存重开后的编辑。记录实际配置、设备和证据及所覆盖的图形合同。

## 5. A 本地验证与交接

- [x] 5.1 A 核对 D06-T06 完成条件、正式受影响目标、V1–V4 和 OpenSpec 严格校验。记录实际命令、源码/产物/fixture 身份、结果、复用及未覆盖项，在 [当前问题单](../../../../Docs/CodeReview.md) 留下实际输入及本地证据，由维护者调度独立会话 B。

## 6. 独立审查、整改与复核

- [x] 6.1 B 核对实际版本、直接消费者、领域身份、文件/资产时序及图回执，完整阅读涉改第一方文件并补齐职责、接口合同及流程注释。在当前问题单记录覆盖、稳定 CR 编号及审查结论。 [独立审查闭环](../../../../AGENTS.md#review-loop) · [注释合同](../../../../.agents/skills/physara-cpp/SKILL.md#annotation)
- [x] 6.2 A 依问题单纠正并自检，记录修改版本和针对性回归；没有整改项时按实际结果登记。
- [x] 6.3 B 复核最终实际源码、回归及注释，确认功能正确性与可读性并记录签收的版本，B 自行确认审查及复核项。

## 7. 最终验证、同步与归档

- [x] 7.1 B 验收后，A 在签收输入上完成最终 verify，执行所需的正式构建、CPU 与原生 GL 检查，匹配的已有结果按证据合同复用。记录实际输入、输出与未覆盖项。
- [x] 7.2 A 在本任务文档留存独立审查、整改、复核及最终验证证据。同步五份 delta，核对原 requirements/scenarios 完整保留并运行严格校验。
- [x] 7.3 A 归档 change 并核对归档路径和开发计划状态，确认验收和证据留存后按模板重置当前问题单。

## 验证方案

| 验证组 | 风险和最小检查 | 停止条件 |
|---|---|---|
| V1 · Scene/编辑与动作 | 用一个带重名节点、非均匀负缩放父级和相机子树的非对称场景，经过实际会话/面板意图选择、局部 TRS 连续预览并撤销、重做，再删除选中节点或绑定相机的子树并重放。使用一处非法字段及一批文本/模态争用或 Repeat，核对取消和单次派发。 | 预期世界坐标由固定输入独立求得，世界值和版本随领域事务变化，未编辑字段不变；重放恢复稳定引用并产生新 handle；拒绝保持已接受值，控件占用时无领域触发。 |
| V2 · 文件与导入发布 | 对同一项目相对资源根用实际文档/资产入口驱动一次 v1 首次保存、继续编辑和重开。给保存后继续制造匹配成功及写入中接受新版本两种结果，并用一个失败或取消观察替换保留。对一个模型请求核对 CpuReady 在手势活动时等待、事务单次提交及重复导入独立；把取消或换文档放在 CPU 结果发布前，检查共享资产仍可正常完成及失效意图无发布。选一项不在准入范围的路径或失效请求，并在代表性的暂停 CPU 轮次核对与槽无关的处理。 | 活动 Scene/路径只由匹配的文件结果更新，稍晚的编辑仍可保存，主 View 导航位姿、可选引用、请求比例按 v1 恢复；早期失败与迟到结果不会添加实体或污染历史。复用 D06-T03 对同格式校验和安全替换的匹配证据。 |
| V3 · 导航、逻辑命中和捕获 | 用固定输入回放三种导航手势，核对右手轴向、轨道距离、Pan 朝向及 Undo/Redo。用一次带非 1:1 实际像素比例和固定相机留边的布局，手算一个图内 UV 和一个边界拒绝，核对暂停或未获正常图确认不允许新命中。覆盖一次同批失焦恢复或隐藏后的捕获归还，经新按下再开始。 | 坐标只映射一次，正常和失效输入走对应的同轮逻辑，只有 View 的相机/文档版本因导航变化；绑定实体相机使用其世界投影且额外 View 会话请求独立。 |
| V4 · 产品与原生 OpenGL | 用正式 CMake 构建受影响的 `Physara` 及直接消费者，Debug/Release 验证正常入口。从其它工作目录启动真实编辑器，以同一小型可区分图案的 glTF/GLB 用实际 UI 完成导入、选择、TRS、删除/撤销、相机导航及隐藏并恢复视口，再通过文件动作保存并重开。比较有明确预期的图像区域或读回像素、活动 Scene/View 身份、采样及提交结果；利用既有记录核对旧图仍在途时正常退休。 | 原生接受的视口与窗口画面随独立坐标和相机预期变化，暂停或隐藏时无替代分配，恢复后同一逻辑视口从最新合法状态准备。记录运行配置、驱动、实际输入和观察结果，旧画面/占位不会被记作新结果。 |

开发材料放在 `build/d06-t06/`，运行正式目标和其实际 Shader 包、资源根及会话路径。固定小场景可跨验证组复用；相同输入、构建和产物身份下只复用已覆盖证据，遇失败、输入变化或具体缺口再追加。正式 OpenSpec 结构检查只证明规划格式。D06-T07 的全阶段集成矩阵和 D07 的物理输出基准按各自工作包执行。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [D06-T07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07)

<a id="a-local-verification"></a>
## A 本地验证

2026-11-17 在 Windows x64、Visual Studio 2022、SDK `10.0.26100.0` 上构建正式 `Physara`，Debug 和 Release 均通过。两种配置均从另一个工作目录运行各自的 `Physara.exe --hidden --poll-limit=6`，并链接相同配置的正式库和 `Runtime/Frame.obj` 运行工作区探针。探针通过固定的宿主事件和 ImGui 输入回放驱动实际控件，用真实 GLFW 宿主确认 Relative/Normal 模式，使用 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 `32.0.101.6129` 执行图和颜色读回。

| 验证组 | 已观察结果 |
|---|---|
| V1 | 重名父子实体可分别选择，隐藏 Hierarchy 时跳过面板内容。Inspector 中的本地平移、旋转和有符号缩放均经字段事务生效，负缩放父级把子实体 X 从 0 移到 −1.6 m 时原生像素也随之移动。单次 Undo/Redo 恢复起点/末值，非有限值及取消后的 Release 无效。Delete 与 Repeat 共用一次事务，文档弹窗中的 Delete 留给控件；撤销删除可恢复稳定相机引用并取得新 handle。 |
| V2 | 文件动作经实际 UI 完成首次保存、普通保存、无路径的新建、打开和未保存时的 Save and Continue。直接会话驱动验证保存中的新修改保留、取消阻止替换、无效 Open 保留活动状态。槽未取得时仍可推进模型和文件；活动手势把 CpuReady 保留到可提交边界，同源导入生成独立实例。取消保留共享 CPU 工作，越界和已失效请求均不实例化，终止不发布待实例。 |
| V3 | 非等比 framebuffer/client 与 0.75 render scale 的输入得到 480×270 的画布和独立预期 UV (0.25, 0.25)。固定相机留边、剪裁和请求改变后的旧画面无法启动导航。在真实接受的主视口核对按下位置、释放边沿、Fly/Orbit/Pan、选中锚点的恒定轨道距离及相机撤销。同批失焦与恢复回放使原位姿复原、真实宿主归还光标，新 Press 才开始捕获。 |
| V4 | 从实际 Import Model 控件提交 glTF，读取了匹配场景输出和窗口合成的红色见证像素。TRS 修改后的图像覆盖改变，隐藏和 Undo 恢复维持同一逻辑 token。重开后的场景稳定身份、TRS 和主 View 位姿/相机/请求比例恢复，额外 View 的会话设置独立。两个配置的场景编辑及保存重开均经过正式正常帧，回放各取得接受票据 104 和 195 的对应帧。 |

本地运行入口为 `python -X utf8 build/d06-t06/a-local/Verify.py Debug` 和 `python -X utf8 build/d06-t06/a-local/Verify.py Release`。两个记录各核对了 1586 项实际正式构建输入，并记录构建、Shader 包、正式库、probe 输入、可执行产物和 fixture 身份。作为源输入的 glTF 与异常 JSON 字节在运行中保持一致。变更正式输入 27 个，按仓库相对路径升序、`path<TAB>sha256<LF>` 的 UTF-8 字节摘要为 `cff3f78e61efd0fe62a97508097af031bdd44f27da9764bc437d6472fa9cdea1`。 [正式变更清单](../../../../build/d06-t06/a-local/a-source-manifest.json) · [Debug 证据](../../../../build/d06-t06/a-local/debug-og0_s16q/evidence.json) · [Release 证据](../../../../build/d06-t06/a-local/release-mtgwcbxl/evidence.json)

OpenSpec 严格校验通过。非等比 DPI 及同批焦点边沿采用固定发布回放，真实画面证据来自上述本地设备；本文只记录 A 本地检查，B 复核后在其签收输入上另行完成最终 verify。

<a id="a-correction"></a>
## A 整改与自检

2026-11-17 完成 CR-001。Inspector 按实际标量项采集同帧失活和激活，工作区保留旧项末值及结束，随后交给会话消费新项的启动或预览。旧项完成后，现有 EditHistory 从权威值建立新手势；同一向量内先绘制新轴时也按相同顺序提交。Escape 及输入失效继续经原字段取消入口。 [审查留存](#review-closeout)

- Debug 与 Release 均通过正式 `Physara` 增量构建及定向 CPU 回放。探针使用固定 ImGui 默认队列、实际 `ScenePanels::DrawInspector` 和会话使用的 `EditHistory::ApplyGestures`。Translation X 输入 `2` 后直接转到 Scale X，旧值和新激活确实同帧到达。将 Scale X 拖到 `3` 后，两次 Undo 和 Redo 分别作用于对应编辑。再测同一行的 Scale Y → X，得到 `(3, 4, 1)`，各轴按完成顺序独立重放。Enter、普通拖拽预览、Escape 回滚和迟到 Release 的行为通过。
- 实际验证命令为 `python -X utf8 build/d06-t06/a-correction/VerifyCorrection.py Debug` 与 `python -X utf8 build/d06-t06/a-correction/VerifyCorrection.py Release`。两次各核对 1586 项正式构建输入及链接库、产品和探针身份。起点匹配 B 注释版。27 个正式输入中有 6 个对应本次手势整改，其余 21 个与 B 注释版字节一致。修正后路径与字节摘要为 `83d518cf0661d014aba8e06c92fc1451484577308a195e312fb448a60ca6004c`。 [修正版输入](../../../../build/d06-t06/a-correction/a-corrected-source-manifest.json) · [Debug 证据](../../../../build/d06-t06/a-correction/debug-9dwe9n84/evidence.json) · [Release 证据](../../../../build/d06-t06/a-correction/release-u05kpvd7/evidence.json)
- OpenSpec 1.13.2 严格校验 32/32 通过。本轮回放覆盖 CR-001 的 CPU 编辑合同及直接消费者构建，原生 V1–V4 最终验证待 B 签收后执行。

<a id="b-recheck"></a>
## B 复核与注释

2026-11-17，维护者调度的独立 Agent-B 完成复核，关闭 CR-001，未发现新增正式问题，并自行确认 6.1 和 6.3。初审覆盖 26 个 C++ 文件和 `Editor/CMakeLists.txt`，职责、接口、生命周期与流程注释记录在[审查留存](#review-closeout)。本次将 A 整改版与 B 注释版逐文件比较：仅 6 个手势相关文件变化，其余 21 个正式输入字节一致。

- 代码与合同：核实 Inspector 按标量控件分开采集完成项与当前项，`ApplyGestures` 先完成旧项再捕获新首值；同一向量逆绘制顺序切轴仍按事务顺序应用。核对 Session、比例控件、导航、Escape/输入失效取消及原历史入口，补充 `ApplyGestures` 对非法末值与部分成功的说明。
- 实际运行：B 核对 A Debug/Release 证据各 1586 项正式输入、22 项产品/项目/链接产物、探针输入和可执行文件身份，并确认原构建/回放日志与记录相符；随后实际运行 `build/d06-t06/a-correction/debug-9dwe9n84/InspectorHandoff.exe`、`build/d06-t06/a-correction/release-u05kpvd7/InspectorHandoff.exe`，均返回 0。Translation X=`2` → Scale X=`3`、同一行 Scale Y=`4` → X=`3` 的独立 Undo/Redo，以及 Enter、普通拖拽、Escape 和迟到 Release 均通过。[Debug 结果](../../../../build/d06-t06/b-recheck/debug-replay.log) · [Release 结果](../../../../build/d06-t06/b-recheck/release-replay.log) · [身份与执行记录](../../../../build/d06-t06/b-recheck/replay-evidence.json)
- 最终签收：HEAD 仍为 `a8cab313e93f7e876559d8ae71f35f7d01ec1721`，签收包含工作区与未跟踪的正式实现。A 修正版摘要 `83d518cf0661d014aba8e06c92fc1451484577308a195e312fb448a60ca6004c` 已核实；B 只追加一行合同注释，27 文件最终摘要为 `172a07aceea9a9bb0abf80d2899869172621b6fea74adccd38de30e25dc3fd4c`，算法沿用 `path<TAB>sha256<LF>`。去除注释后的代码/token 不变，`git diff --check` 和链接检查通过。[签收清单](../../../../build/d06-t06/b-recheck/accepted-source-manifest.json) · [B 注释差异](../../../../build/d06-t06/b-recheck/b-recheck-annotation.diff)
- 结论与限制：代码正确性及注释可读性均通过 B 复核。复用 A 身份匹配的 Debug/Release 正式构建，本次 B 未重跑原生 V1–V4；第 7 节最终 verify、同步、留存及归档仍由 A 完成，保持未勾选。签收后改变代码或构建输入须交 B 复核受影响范围，问题单在最终验证及归档成功前保留。

<a id="final-verification"></a>
## 最终验证

2026-11-17，A 在 B 签收的 27 个正式输入上执行最终 verify，路径与原始字节摘要为 `172a07aceea9a9bb0abf80d2899869172621b6fea74adccd38de30e25dc3fd4c`。初审和整改快照、B 的签收记录、下列构建和运行使用的 1586 项正式输入均已重新核对。最新签收相对 A 整改版只变更 `EditHistory.hpp` 的一行合同注释，实际产品与探针仍按最终输入重建。 [签收清单](../../../../build/d06-t06/b-recheck/accepted-source-manifest.json) · [最终记录](../../../../build/d06-t06/final/signed-run-summary.json)

| 维度 | 核对结果 |
|---|---|
| 完整性 | spec-driven 提案、设计、任务及五份增量齐备。实施、本地验证与独立审查任务均已完成。本节验收 7.1，后续办理主规范同步及归档。 |
| 正确性 | 五份增量的 8 项需求及 32 个场景与实际实现对应，按下表归入 V1–V4 和 CR-001 的代表性运行。Debug/Release 正式构建、原生 GL 工作流和 CPU 字段交接均通过。 |
| 一致性 | 层级与 Inspector 消费 Scene 权威值，文件与实例沿会话的 CPU 边界发布。导航、准备及输出确认保持 View 的逻辑身份与图回执合同。JSON v1 由现有文档层读写，代码分层与设计一致。 |

| 能力与场景范围 | 主要实现及证据 |
|---|---|
| editor-domain-edits，3 项、12 个场景 | V1 核对重名选择、带负缩放父级的本地 TRS、删除/相机回退及撤销；V2 核对同源导入、CpuReady 等待、取消、路径拒绝与请求代失效。CR-001 核对旧末值同帧提交以及逆绘制顺序切轴，两次编辑按顺序独立重放，Enter、拖拽与 Escape 通过。 [Inspector](../../../../Editor/Panels/ScenePanels.cpp) · [历史](../../../../Editor/Commands/EditHistory.cpp) · [导入](../../../../Editor/Session/SceneImport.cpp) |
| editor-session-input，2 项、7 个场景 | V1 检查文本、模态、菜单及快捷键派发。V3 从接受图像发起 Fly、Orbit、Pan，真实 GLFW 模式观察确认捕获及归还。固定输入核对按下位置、同批失焦、释放和场景相机门控。 [动作](../../../../Editor/Panels/Workspace.cpp) · [导航](../../../../Editor/Viewport/ViewportNavigation.cpp) |
| editor-viewport-output，1 项、5 个场景 | V3 核对单次比例及坐标映射、留边和旧请求失效；V4 核对隐藏及恢复后的相同 token、准备与接受、实际视口和窗口像素。 [映射](../../../../Editor/Viewport/Viewport.cpp) · [正常图确认](../../../../Runtime/Frame.cpp) |
| runtime-lifecycle，1 项、5 个场景 | 正式程序从另一工作目录执行 `--hidden --poll-limit=6`，空文档启动及受控退出均返回 0。V2 检查不取得帧槽仍推进 CPU 资产/文件、会话终止后迟到导入失效。 [Runtime](../../../../Runtime/Main.cpp) · [会话](../../../../Editor/Session/EditorSession.cpp) |
| scene-document-persistence，1 项、3 个场景 | V2 和 V4 用 UI 完成 New/Open/Save/Save As、保存后继续、写入期间新编辑保留以及 v1 重开；用失败和取消输入检查匹配发布。 [文件 UI](../../../../Editor/Panels/DocumentPanel.cpp) · [文件协调](../../../../Editor/Session/SceneDocuments.cpp) |

正式构建使用 Windows x64、Visual Studio 2022 和 SDK `10.0.26100.0`。两配置使用同设备 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0，驱动 `32.0.101.6129`。既有 glTF/JSON fixture 生成后逐字节核对，复用实际 `WorkspaceProbe.cpp` 与 `InspectorHandoff.cpp`。每个配置一次正式增量构建，复用该配置的新产物做各自原生和定向 CPU 检查。实际窗口帧在导入、TRS 修改、隐藏/恢复与保存重开时由正常图接受，V4 末端回执分别为 Debug `195`、Release `197`。 [Debug 原生证据](../../../../build/d06-t06/final/workspace/debug-xyrvugcr/evidence.json) · [Release 原生证据](../../../../build/d06-t06/final/workspace/release-2bp5ir2x/evidence.json) · [Debug 交接回归](../../../../build/d06-t06/final/correction/debug-jc2yt1xb/evidence.json) · [Release 交接回归](../../../../build/d06-t06/final/correction/release-z8rlzilc/evidence.json)

入口为 `python -X utf8 build/d06-t06/final/VerifySigned.py Debug Release`。首轮 Debug 已完成构建和原生流程，汇总脚本随后因历史日志字段名中止。修正规则后，以 `--reuse-native build/d06-t06/final/workspace/debug-xyrvugcr` 核对并复用这次同身份的 Debug 结果，再完成 Debug CPU 回放及 Release 全部检查。最终汇总摘要为 `dffc6cb3837969b12ab5d7796537bcf95ddfbe952dbb695ae0826cee2d854224`。

代表性输入覆盖本包风险。非等比 DPI 及同批焦点边沿使用固定发布回放，原生图像与宿主模式验证限于上述设备。窗口和视口独立度量、在途退休及既有文件合同由直接消费者与历史合同一同核对，D06-T07 承担阶段集成门槛的扩展覆盖。 [后续阶段门槛](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07)

<a id="review-closeout"></a>
## 审查留存与规范同步

| 问题 | 原复现与影响 | 修正、复核及证据 |
|---|---|---|
| CR-001 [P2] | Translation X 文本输入 `2` 后直接点击 Scale X，同帧旧项末值被新激活覆盖，Translation 仍为 `0` 且没有 Undo 记录。Enter 正常提交是对照。 | 逐轴获取旧完成与新活动信息，会话先提交旧末值，再以实际状态开启新事务。A 和 B 的定向回放均通过，签收版 Debug、Release 复跑验证字段切换、同一向量逆绘制顺序切轴、独立 Undo/Redo、Enter、拖拽和 Escape。CR-001 已关闭，无新增正式问题。 [初审失败](../../../../build/d06-t06/b-review/interaction-result.txt) · [初审对照](../../../../build/d06-t06/b-review/interaction-control.txt) · [B 复核](#b-recheck) · [最终验证](#final-verification) |

B 初审覆盖 26 个第一方 C++ 文件及 `Editor/CMakeLists.txt` 的职责、接口和状态推进。包含层级投影、字段编辑历史、文档与资产实例的取消及发布、View 请求/导航/命中、宿主输入 bank 和终止边界、正常图接受及窗口呈现。复核 A 的 6 个改动文件及直接消费者，确认其余 21 个正式文件与 B 初审注释版字节一致，额外完善 `EditHistory.hpp::ApplyGestures` 的失败状态说明。B 已分别签收功能正确性和注释可读性，实际回放、复用和未覆盖范围见本文件的复核记录。 [B 签收快照](../../../../build/d06-t06/final/review-signed.md) · [B 初审注释](../../../../build/d06-t06/b-review/b-annotations.diff) · [B 复核注释](../../../../build/d06-t06/b-recheck/b-recheck-annotation.diff)

2026-11-17，已将五份 delta 合并到 `editor-domain-edits`、`editor-session-input`、`editor-viewport-output`、`runtime-lifecycle` 和 `scene-document-persistence`。共新增 4 项需求、修改 4 项需求，保留修改项中的 12 个既有场景并加入 20 个场景。未涉及的需求和 Purpose 按原顺序、原内容保留。主规范与全部增量重新比对一致，OpenSpec 1.13.2 主规范严格校验 31/31 通过。

### 归档

2026-11-17，OpenSpec 1.13.2 将 `edit-static-scenes` 按 `spec-driven` 归入 `2026-11-17-edit-static-scenes`。五份增量和 `.openspec.yaml` 保留原始字节。提案、设计与任务的相对链接按归档目录调整。开发计划登记 D06-T06 已验收归档，D06-T07 前置齐备。 [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)

已核对签收源码、既有归档及主规范。最终问题单留存于 build，工作区问题单按模板逐字节恢复。本 change 完成 20/20。 [归档前问题单快照](../../../../build/d06-t06/final/review-closeout.md)
