# Tasks

## 1. 固定输入与正式运行环境

- [x] 1.1 核对 D05 出口及 D06-T02/T05/T06 归档，查验能复用的本地材料。按 [验证矩阵](design.md#verification-matrix) 登记本轮实际源、构建配置、正式资源包及前置证据范围，记录各组待补的证据和匹配已有结果的条件。
- [x] 1.2 在 `build/d06-t07/` 准备同一 glTF/BIN/PNG、小型坏依赖场景、隔离的运行资源根及输入回放。记录生成方式、预期解码值、窗口标样和容差。以正式构建说明核对 Debug/Release 的包与可执行文件身份、fixture 树和现有入口能从指定目录使用。

## 2. 标准输入和场景边界

- [x] 2.1 执行 V1 的 Asset 检查。由正式模型请求发布完整的固定 CPU 记录，逐个对应多 Primitive/材质、两套 UV、sparse 替换与未替换值，以及缺源切线时的角点索引和手性。使用输入独立预期判定，记录资产和依赖版本。
- [x] 2.2 执行 V1 的场景检查。对同源同选项重复请求核对固定模型版本，创建两个独立实例并给其中一个不同的有符号 TRS。沿 Scene 快照和 SceneFrame 入口核对独立实体/选择、引用与变换，并确认 Scene 只持领域及 CPU 数据，GPU 对象归渲染与图形 owner。

## 3. 原生图像和编辑工作流

- [x] 3.1 执行 V2 的原生图像检查。通过正式编辑器导入和选相机，使用已知样本核对 UV、稀疏轮廓、镜像面/法线、MASK 孔及近远遮挡，验证正交/有限透视和有效矩形。留存代表性 Base Color 与 World Normal 图像及标样，关联同一帧实际上传和 Draw、接受的 View/UI 采样、默认后缓冲读回、图形接受票据及真实交换。
- [x] 3.2 执行 V2 的场景操作。在真实界面修改一个实例的 TRS，执行 Undo/Redo 与 Save As，再实际 Open 保存的 JSON。以稳定身份、源版本、local TRS、主 View 的相机与请求比例以及另一实例状态核对恢复，观察重开后继续一次编辑所获的新窗口画面。

## 4. 取消、失败、尺寸变化及退出

- [x] 4.1 执行 V3 的取消、重试和失败检查。在 Loading 以后取消会话操作，再允许共享完成被处理。以取消操作的零场景事务、同源再次导入的独立提交、坏依赖的可见失败与旧场景保持为通过条件。确认 CPU 记录完整性、版本匹配及恢复后的合法显示。
- [x] 4.2 执行 V3 的终止分支。使关闭输入到达时仍有模型作业运行或结果待取，观察同一实际 Runtime 在终止轮次截止采用与 CPU 发布、取消并 join、收拢图形工作并正常退出。记录触发时实际作业状态、真实宿主的退出结果及受控步骤。
- [x] 4.3 执行 V4。在有已接受且待退休的静态场景帧时，删除一个实例并连续改变 View extent，验证真实窗口 resize 后的重新取得。记录稳定 token、旧输出与旧 GPU 引用、下一个匹配的新 View/窗口结果，以及旧提交完成至槽安全复用的顺序。核对删除/撤销后的像素和另一实例状态，分别记录受控 Pending 与真实完成。

## 5. A 本地验证与交接

- [x] 5.1 A 对照 [D06 阶段出口](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07) 核对 V1–V4、正式 Debug/Release 目标、同包产品短运行和 OpenSpec 严格校验。记下命令、实际输入与产物身份、支持 profile、已复用证据和未覆盖项。检查各组有效条件已满足后，在 [当前问题单](../../../../Docs/CodeReview.md) 写明交接输入与证据，由维护者调度 B。

## 6. 独立审查、整改与复核

- [x] 6.1 B 按维护者本轮限定的源码范围，独立核对正式消费者、关闭与退休实现，完整阅读涉改第一方文件并按合同补齐注释；回放输入、标样及原生观测引用 A 已记录的证据摘要，本轮不审核 build 测试材料。在当前问题单记录覆盖、正式发现及限制。 [审查闭环](../../../../AGENTS.md#review-loop) · [注释规格](../../../../.agents/skills/physara-cpp/SKILL.md#annotation) · [B 复核记录](#b-recheck)
- [x] 6.2 A 依问题单整改并自检。保留失败复现及等口径回归，记录实际改动与证据，标注受影响的验证组。若 B 无整改项，按实际结论记录本步。
- [x] 6.3 B 复核 A 接收与自检、最终注释及实际源码身份，在问题单签收源码与注释版并勾选 B 负责项。正式发现 0 项，功能整改不适用；按维护者限定范围，本地验证输入及原生产物对应关系由 A 在 7.1 核对，B 的证据引用与实测限制见 [复核记录](#b-recheck)。

## 7. 最终验证与归档

- [x] 7.1 B 验收后，A 在签收输入上完成最终 verify，核对 V1–V4 与正式产品运行证据均匹配当次构建和实际产物。补足有影响的差异或缺口，记录签收版在真实 OpenGL 上的结果、复用和未覆盖项。
- [x] 7.2 将独立审查、整改与最终验收保留于本 tasks。完成 OpenSpec 严格校验，核对 `skip_specs` 与实际合同范围一致，并按最终元数据处理主规格同步。
- [x] 7.3 在验收条件全部满足后归档，核对归档和开发计划链接，标记 D06-T07 与 D06 阶段出口并记录 D07 的入口状态。确认归档及证据留存后按模板恢复当前问题单。

<a id="verification-evidence"></a>
## 验证证据

每组取证随所属任务记录配置、重放命令和输入身份、预期与实际结果。原生组还记录设备和驱动、帧与票据关联、真实交换、完成和观察限制。留存复现场景布局、固定数据生成方式与本地材料清单，注明前置证据的适用范围及删除 build 后需要由交接提供的输入。使用 [当前支持 profile](design.md#support-profile) 汇总当次验收范围。

### 实施基线

- HEAD 为 `81b7beed292707d848245a04f10fe5fb05b7f3f4`。正式目录的实际工作树与该提交一致。D05 及 T02/T05/T06 的归档证据继续覆盖各自已签收场景。对应的历史本地材料已清理，本轮 V1–V4 用新的合成输入及同配置正式对象重新运行。
- Visual Studio 生成器在查询本机 SDK 时遇到目录访问限制。改用并行配置的 Ninja Multi-Config，由 MSVC 19.44.35228、SDK 10.0.26100.0 独立构建 Debug/Release 的正式 `Physara` 及四份实际 Shader 包。1212 个正式输入的字节摘要和构建日志见 [本地基线](../../../../build/d06-t07/a-local/baseline.json)。

<a id="a-local-evidence"></a>
### A 本地验证

从仓库根用 `& build/d06-t07/checks/Build.cmd configure` 和同一命令的 `Debug`、`Release` 参数构建。运行 `python -B -X utf8 build/d06-t07/checks/Verify.py Debug` 和 `Release` 复跑。脚本为每次执行建立新的资源根，逐项核对正式输入、同配置对象和包、生成输入、编译产物及日志。探针链接正式 `Frame.cpp.obj`、`Resources.cpp.obj` 和产品链接依赖，两个配置的正式 `Physara.exe --hidden --poll-limit=6` 均从独立工作目录退出 0。 OpenSpec 1.13.2 严格校验 32/32 通过。 [构建与回放入口](../../../../build/d06-t07/checks/Verify.py)

样例为本地合成的 `Stage.gltf`、一份外置 BIN 和两张 4×4 PNG。源切线缺席，前层 MASK 使用 UV0 基色与 UV1 法线，后层 Unlit 作为遮挡参照。透视和正交的固定比例同为 2:1，眼点为 `(1, 0, 4)`，正交半宽/高为 `3.2/1.6`。透视垂直视场为 `2*atan(0.4)`，两者在前层平面投影同宽。坏资产仅缺 BIN。实测 RGB 以预先确定的 sRGB 样本比较，容差为各通道 3。正交和有限透视的 Reverse-Z 深度容差为 0.003。 [生成输入及坐标](../../../../build/d06-t07/checks/Fixture.py) · [判定代码](../../../../build/d06-t07/checks/Gate.cpp)

| 检查 | Debug 和 Release 实际结果 |
|---|---|
| V1 标准输入及双实例 | 同一 CPU revision 1 与四个源依赖整体发布。sparse POSITION 从 `(.25,.75,0)` 变为 `(1.25,.75,0)`，其余哨兵和双 UV 符合预期。UV1 镜像接缝的两角点各拆分一份，生成六个顶点及两种切线手性。Editor 分别选择、编辑 `entity-1` 和 `entity-5`，SceneFrame 共享一个固定 CPU 模型、四个 Primitive 引用和两种绕序。 |
| V2 原生画面及文档 | 首次接收静态颜色的 Debug/Release 票据为 24/9，两种配置各观察到 2 次模型贴图上传、11 次缓冲上传、2 次场景索引绘制以及 UI 对该 View 存储的实际采样。实际 Win32 交换成功。后续帧的已接受 View 与原生默认后缓冲均核对稀疏轮廓、两 UV、镜像面/法线、MASK 孔和前后遮挡。两类投影的留边与深度按各自预期通过。经 Inspector、Undo/Redo、Save As 和 Open 恢复原稳定实体、模型指纹、非均匀镜像 TRS、相机 `entity-3` 及渲染比例 0.74。重开前后及继续编辑的特征像素在新票据出现。 |
| V3 取消、失败及退出 | Loading 时用 `Scene status` 取消采用，随后共享 CPU 完成仍是完整记录，原 Scene 事务为零。再次导入共用该固定版本。缺失 BIN 显示 `Import failed` 和 `/buffers/0/uri` 的实际原因，当前场景和资产保留，窗口继续呈现。另一个模型在 worker 实际 `ReadFile` 内受控等待，正式 `RunEditor` 收到宿主关闭请求，终止轮次无发布、无新提交或交换。随后会话停止、作业被取消并 join，原生宿主关闭成功。 |
| V4 删除、resize 与退休 | 对三个实际提交的 fence 暂缓完成观察，两个配置各观测到 75 次受控 Pending。Delete 与两次比例编辑期间保留旧颜色代和原生引用，旧图失去输入资格。真实窗口 framebuffer 从 1000×720 改为 900×650，恢复后采用同 token `4294967296` 的新代。Debug/Release 的观察票据分别为 258→260 和 88→90。恢复原生完成查询后，旧 View 和窗口纹理均在已明确完成的票据之后实际删除。像素证明删除与撤销，另一实例保持原状。 |

| 配置 | 实际执行清单 | 原生画面 |
|---|---|---|
| Debug | `d4ce6074c5c53dd52a4c62662b2f341fcace02d3f2e9959d15edcaeba2632908` [清单](../../../../build/d06-t07/a-local/debug-v_uwkd__/evidence.json) · [日志](../../../../build/d06-t07/a-local/debug-v_uwkd__/gate.log) | [Base Color](../../../../build/d06-t07/a-local/debug-v_uwkd__/base-window.png) · [World Normal](../../../../build/d06-t07/a-local/debug-v_uwkd__/normal-window.png) |
| Release | `1f8dd404709363f22c54d49a7e65b5541447ce551cc9eabc313a06d59007e9de` [清单](../../../../build/d06-t07/a-local/release-oghpngxz/evidence.json) · [日志](../../../../build/d06-t07/a-local/release-oghpngxz/gate.log) | [Base Color](../../../../build/d06-t07/a-local/release-oghpngxz/base-window.png) · [World Normal](../../../../build/d06-t07/a-local/release-oghpngxz/normal-window.png) |

本轮实机为 Windows x64、Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0（驱动 32.0.101.6129）的单设备隐藏 sRGB 窗口。回放使用正式 ImGui 控件与会话的输入队列，焦点为回放设定。受控 Pending 用真实 fence 进行后续收拢。GLB、其它拓扑、材质通道和自发光按前置归档的既有范围引用。其它设备/驱动、跨屏和自然 GPU 挂起未覆盖。本轮正式实现差异为零。交接的实际文件身份和开发材料入口见 [审查留存](#review-closeout)。

<a id="a-review-annotation-check"></a>
### A 接收 B 审查与自检

B 第 1 轮完成源码审查及注释，正式发现 0 项。A 记录功能整改不适用，并接收四处注释增补。 [审查留存](#review-closeout)

- `Runtime/Frame.cpp` 的场景全集判定与 Forward 的清屏行为一致，解释了有效相机下空可见集仍使用正常场景背景的原因。 [Forward 规格](../../../specs/static-forward-rendering/spec.md)
- `SceneFrame.cpp` 的缓存边界说明与 Scene 的内容版号及 ViewWork 单独准备相机一致。 `View.hpp` 的历史记录说明与 `View::Publish` 和工作区显示使用的 `AcceptedScenePreview` 一致。 [帧提取与视图准备](../../../../Docs/Architecture.md#prepare) · [视口输出](../../../specs/editor-viewport-output/spec.md)
- `StaticDraw.cpp` 对法线槽的朝向矩阵、绝对尺度及零轴解释与共享顶点 Shader 和 D06-T05 已采纳设计一致。 [法线采样设计](../2026-11-16-render-static-scenes-forward/design.md)

对比 A 本地验收的 1212 项正式输入，只有上述 4 文件变化。当前差异为 7 行独立注释，去除新增行并统一行尾后与基准逐行相同。核对了宏、预处理指令及续行边界。四文件原始字节的集合 SHA-256 为 `ad0e353638901f30e03bda84c4bf2f2c021dfbc788b7c683e0cf7ab3b8e629c1`，按相对路径排序并以 `path<TAB>sha256<LF>` 计算。 [注释版输入清单](../../../../build/d06-t07/a-correction/annotated-source.json)

注释自检按 L0 完成，OpenSpec 严格校验 32/32 通过。V1–V4 仍对应上方记录的 A 实测输入。B 本轮结论限于源码与注释，最终签收及签收版原生 verify 分别按 6.3 和 7.1 处理。

<a id="b-recheck"></a>
### B 独立复核与源码签收

维护者于 2026-11-17 再次指派 B 复核 A 的交接，延续“审查正式渲染器代码、注释及问题单，不扫描 build 繁杂材料”的范围。本节及 6.1、6.3 由 B 填写，A 的 6.2 与本地运行记录保持其原有归属。任务措辞同步到该审查范围，V1–V4 和最终原生 verify 的验收要求不变。

- **版本：** 分支 `v2.0`，HEAD `81b7beed292707d848245a04f10fe5fb05b7f3f4`。当前生产差异仅为 `Runtime/Frame.cpp`、`Engine/Renderer/Prepare/SceneFrame.cpp`、`Engine/Renderer/View.hpp`、`Engine/Renderer/Draw/StaticDraw.cpp` 中的 7 行注释，无新增生产文件、功能改动或 CMake/Shader 改动。文档、计划及本 change 的工作树变化另行保留。
- **身份复核：** B 按 A 声明的相对路径排序与 `path<TAB>sha256<LF>` 算法直接读取四文件原始字节，独立得到集合 SHA-256 `ad0e353638901f30e03bda84c4bf2f2c021dfbc788b7c683e0cf7ab3b8e629c1`，与 A 交接完全一致。该摘要覆盖四份注释文件；其余受版本控制的生产输入由 HEAD 加实际 diff 核对，无需读取 build 清单。
- **源码与注释：** 复用同版本的 B 首轮完整阅读，再次核对四处注释及直接消费者：场景全集与 Forward 空可见集清屏，Scene 内容版号与逐轮相机准备，历史场景记录与当前接受显示色，normal UV 的 Q 朝向及顶点 Shader。注释准确，无需继续修改。正式发现 0 项，功能整改不适用。
- **B 实际检查：** 逐行差异仅包含 7 行独立注释，原代码保持相同；新增行及其前一行均无反斜线续行，不改变宏或预处理语义。文件摘要匹配、`git diff --check`、问题单及本 tasks 的仓库文档路径/锚点检查通过。未新增行为测试。
- **证据与限制：** 原生运行引用上方 A 已记录的 Debug/Release、V1–V4、产品短运行及其设备/时序限制；未独立审核 fixture/标样、读取 build 日志或复跑 GPU。B 本轮未运行 OpenSpec validate，当前 shell 未找到其命令，32/32 属于 A 的自检结果。B 签收源码与注释版，不把原生摘要引用视为独立实测或最终 verify。
- **结论：** 源码正确性与注释可读性复核通过，6.1、6.3 完成，允许 A 在此签收版上继续 7.1。A 须将实际验证输入、构建和执行产物与签收版对应，明确复用及未覆盖项，再完成留存和归档。后续生产代码或构建输入发生变化时重新交 B 复核；本节不勾选 7.1–7.3，也不宣布 D06 阶段出口完成。

<a id="final-verification"></a>
## 最终验收与 D06 阶段出口

2026-11-17，B 在 [独立复核记录](#b-recheck) 签收源码与注释，正式发现 0 项。A 在基准 `81b7beed292707d848245a04f10fe5fb05b7f3f4` 上核对四份注释文件的集合摘要 `ad0e353638901f30e03bda84c4bf2f2c021dfbc788b7c683e0cf7ab3b8e629c1`。1212 项签收版正式输入的集合摘要为 `518e66cc5fbad72d7d8a62fe765beef0df3eec78eb259649ed39b93563e26a88`。构建与执行前后均核对实际输入，源码和注释范围与 B 签收一致。 [签收问题单快照](../../../../build/d06-t07/final/review-signed.md) · [签收版输入](../../../../build/d06-t07/final/baseline.json)

从仓库根运行 `python -B -X utf8 build/d06-t07/final/VerifySigned.py`。使用 Ninja Multi-Config、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0，在独立目录重新配置、构建正式 Debug/Release 产品、Shader 包及对应的原生探针。回放复用经过摘要匹配的原探针、样例生成器和标样，每个配置的 15 份资产输入与原 A 回放字节一致。两种配置均执行了独立工作目录中的 `Physara.exe --hidden --poll-limit=6` 和完整 V1–V4。 [验证入口](../../../../build/d06-t07/final/VerifySigned.py) · [签收版运行总表](../../../../build/d06-t07/final/signed-run-summary.json)

| 核对项 | 签收版 Debug 和 Release 结果 |
|---|---|
| V1 静态输入与实例 | 通过。相同 CPU 固定版本恢复 sparse 顶点、两套 UV、拆点和切线手性。双实例复用模型正文，各有独立变换和实体身份。 |
| V2 像素与文档 | 通过。首次静态场景票据为 21/26，记录实际纹理与缓冲上传、Draw、UI 采样及成功交换。两类投影、Reverse-Z、遮挡、镜像和法线标样均在实际接受的 View 与窗口后缓冲对比中通过。保存重开并继续编辑后在新接受帧观察到对应图像变化。 |
| V3 取消、失败与退出 | 通过。迟到的共享完成未生成已取消意图的实例，合法重试使用完整版本，坏依赖保留已有场景。真实 worker 的读取被受控挂起，关闭只收拢在途工作，终止轮次无新 CPU 发布、提交或交换。 |
| V4 删除、resize 与退休 | 通过。窗口从 1000×720 调整为 900×650。旧图与窗口纹理在受控 Pending 期间保活，恢复后由真实 fence 完成并观测原生删除。对应票据 Debug 为 254→256，Release 为 198→200。View token 稳定，当前输出和删除、撤销后的像素匹配。 |

| 配置 | 正式构建、产品短运行及原生回放 |
|---|---|
| Debug | 均通过。 [构建日志](../../../../build/d06-t07/final/build-debug.log) · [运行清单](../../../../build/d06-t07/final/debug-yhyg3k3w/evidence.json) · [原生日志](../../../../build/d06-t07/final/debug-yhyg3k3w/gate.log) |
| Release | 均通过。 [构建日志](../../../../build/d06-t07/final/build-release.log) · [运行清单](../../../../build/d06-t07/final/release-vpv_dxo3/evidence.json) · [原生日志](../../../../build/d06-t07/final/release-vpv_dxo3/gate.log) |

| 核对维度 | 结论 |
|---|---|
| 完整性 | D06-T07 的三个计划目标、A/B 闭环及 7.1 原生验证有对应记录。D06-T01 至 T06 的归档验收纳入阶段出口。 |
| 正确性 | 正式入口完成从 glTF 输入、场景编辑与文档、图准备、RHI 和 UI 到窗口呈现的纵向链。增量需求和新增场景映射不适用，OpenSpec 1.13.2 归档前严格校验 32/32。 |
| 一致性 | 依照设计复用正式所有者以及同一帧图与主入口。B 的七行注释说明与直接消费者合同一致，范围匹配工作区实际差异。 |

实机证据覆盖单台 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 Build 32.0.101.6129 的隐藏 sRGB 窗口。使用真实 native 交换、查询和退出。Pending 与工作线程读取的观察时序由探针控制。其它设备、跨屏和自然 GPU 挂起仍沿现有覆盖边界记录。GLB 等前置范围沿用各自归档证据。 [支持 profile](design.md#support-profile)

<a id="review-closeout"></a>
## 审查留存与规范处理

B 初审和复核在 [独立复核记录](#b-recheck) 留存，A 的接收结论为功能整改不适用。B 审查范围为正式源码和注释。回放输入、构建和原生运行的身份由 A 的签收版最终验证核对。所选变更声明 `skip_specs: true`，OpenSpec 返回的增量路径集合为空，主规范沿用既有 31 份。 [A 自检](#a-review-annotation-check) · [最终结果](#final-verification)

### 归档

2026-11-17，OpenSpec 1.13.2 将 `verify-static-asset-to-window` 按 `spec-driven` 归档。归档后主规范严格校验 31/31，提案、设计和任务的相对链接已按新位置调整。D06-T07 三项计划目标及 D06 阶段出口均已验收，开发计划登记 D07-T01 前置已满足。 [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)

归档前的问题单快照与 B 签收版分别留存。确认归档及验收证据后，已按空模板逐字节恢复工作区问题单。本 change 完成 16/16。 [归档前问题单](../../../../build/d06-t07/final/review-closeout.md)
