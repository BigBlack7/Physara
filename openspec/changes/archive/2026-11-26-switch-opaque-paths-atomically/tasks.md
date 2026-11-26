# Tasks

## 1. View 请求及文档往返

- [x] 1.1 将路径放入 View 的类型化权威请求，通过既有主 View 字段事务和显式离屏入口提交。保留合法值域、初始 Forward、同值幂等和身份检查。用 V1 核对请求／有效状态分离、版本及 Undo/Redo，保留相机与 AA 的现有合同。
- [x] 1.2 在 v1 文件边界解析及写出可省略的 `mainView.requested.opaquePath`。用 V1 核对缺省和拒绝条件，以及保存仍待选的 Forward+ 后重开的请求值和首次确认。沿用现有 Save 快照和完整 Open 发布。

## 2. 计划准备、选择与接受

- [x] 2.1 按 View/Scene 和当前请求身份跟踪待选计划及原因，在帧边界选择封存的候选或用实时输入准备原有效策略。复用同一 SceneFrame 和实际角色键。用 V2 验证连续相机移动、重复／往返请求、离屏独立性和两种支持的 AA 配置。
- [x] 2.2 将实际有效和待选的需求交给已有 Shader/资源准备及预算，保持其它 View 的消费者可用。只有选用的计划驱动图中的私有写入和相关绑定。用 V2/V3 验证待选缺项及编译拒绝期间原有效计划能继续使用，恢复时选用兼容资源及代。
- [x] 2.3 从本轮实际选用的计划、正常采样或离屏导出和提交回执一起发布路径、输出、相机及 Shader 代。按实际 View/Scene/票据交付统计与延迟计时。用 V2/V4 核对正常确认，以及安全 UI、无采样或未接受尝试的发布资格。

## 3. 兼容性与资源生命周期

- [x] 3.1 用已接受的生产身份区分兼容位姿更新与路径、resize 等不兼容变化。验证失效作用于实际接受的 View 和帧，保留旧色读取与相关映射的现有规则。用 V2/V4 核对接受和未接受的对照。
- [x] 3.2 沿现有描述、依赖与票据保活规则复用共享驻留、Pipeline 和兼容图资源。用 V4 核对原生提交、旧输出及相关资源的退休，同时核查切换调用链无重载、全设备等待或全量重建。

## 4. 编辑器及 A 本地交接

- [x] 4.1 从 View Settings 提交当前主 View 的字段意图并展示请求、真实有效路径及原因。用 V1 核对字段编辑、文档冻结和首次确认，结合 V2/V3 的真实帧计划核对面板状态与诊断模式。
- [x] 4.2 记录 V1–V4 实际运行与复用证据、工作树与产物身份、适用限制及已完成范围。将本地记录和问题单交给维护者调度 B，B 的审查任务保持待办。 [签收问题单](../../../../build/d10-t04/final/review-signed.md)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核对实际改动、效果合同与直接消费者，阅读并完善相关第一方文件注释。用稳定 CR 编号记录发现、覆盖情况和本轮结论。第 1 轮核对 16 个产品文件，5 组原生回放与接口审查发现 CR-001 至 CR-006，待 A 整改。[问题单与注释覆盖](../../../../build/d10-t04/final/review-signed.md)
- [x] 5.2 A 完成第 1 轮整改，以及 CR-002 和 CR-003 的第 2 轮复修与自检。 [第 1 轮 A 整改](#a-correction-1) · [第 2 轮 A 整改](#a-correction-2)
- [x] 5.3 B 独立复核各项整改、实际回归证据及最终注释。第一次复核通过 CR-001、004、005、006，第二次复核通过 CR-002、003。六项均已关闭，代码和注释已签收。[第一次复核](#b-recheck-1) · [第二次复核及签收输入](#b-recheck-2)

## 6. 最终验证、同步与归档

- [x] 6.1 对齐 B 签收源码与原生输入，完成两配置构建、启动及最终验证。 [最终验证](#final-verification)
- [x] 6.2 已同步 3 个增量规格，核对完整修改块、既有要求及场景，主规格严格校验 43/43 通过。 [规格同步](#spec-sync)
- [x] 6.3 使用 OpenSpec CLI 归档，核对状态与引用，更新 D10-T04 验收及 D10-T05 前置记录。保留签收与验收证据后恢复问题单空模板。

<a id="verification-scope"></a>
## 验证口径与停止条件

| 编号 | 改变的风险 | 最小有效检查 |
| --- | --- | --- |
| V1 · 编辑与文档 | 面板误认请求已生效、Undo 另存一套状态、旧 v1 或非法值破坏完整打开 | 从正常编辑入口提交路径并撤销重做。以待选 Forward+ 请求与已确认 Forward 为对照，核对 View revision、保存快照、旧文件缺省、待选请求重开及一个显式非法字段的原子拒绝。复用已有冻结／替换场景操作，核对路径字段经过同一事务。观察当前请求、有无有效路径及准备原因。 |
| V2 · 请求取代与双 View 连续渲染 | 切换期间画面冻结、迟到结果替代新请求、误用旧相机或跨 View 归属错误 | 用真实 OpenGL 正常帧让主 View 和离屏 View 同享 SceneFrame 并交错改路径。一条当前未就绪的实际 Pipeline 提供 Pending，期间移动对应相机并发起 Forward → Forward+ → Forward → Forward+。用图中选用角色、实际颜色消费者、接受票据及相机核对旧策略先使用当前输入继续出图，之后只确认匹配请求。覆盖正常受光的 None 与 FXAA，并核对一份跨切换到达的 GPU 计时保有原身份。 |
| V3 · 编译失败与恢复 | 失败被吞掉或改变有效代、另一 View 也被阻塞、错误修复后沿用陈旧候选 | 在开发输入副本中使未就绪的待选路径所需 Shader 角色实际编译失败，记录程序、变体和可定位原因。通过正常帧核对仍可执行的有效路径交付新票据，另一 View 可完成自己的准备工作。修复该实际错误，核对最新请求成功交付。沿已有角色包与预算验证，确认未选 Forward+ 不提交 Froxel 写入。 |
| V4 · 提交与保活 | 把帧前就绪等同成功、旧栅格候选误确认、在途存储被覆盖或无谓换代 | 在一条 View 等待切换期间调整尺寸，核对有效策略先接受可准备的新栅格，随后新路径只用该栅格确认。以一次失去主视口实际消费的候选作反证。部分执行票据的发布拒绝优先复用当前实现有覆盖的原生证据，有缺口时补一项可达的普通执行 Error。按实际票据对照兼容性、另一路的记录、共享驻留与保留的旧色，验证针对实际旧引用的读取和完成后退休。 |

优先复用 D10-T03 已验收的 Shader 角色、图依赖和输出证据，从正常产品构建与受影响入口开始。运行身份至少包括源与构建输入、可执行文件、实际包、场景配置及设备／驱动。故障注入和开发探针仅放 `build/d10-t04/`，正式入口按生产合同运行。检查已证明当前风险且相同输入的证据可定位时停止。若改变已签收的代码或构建输入，按闭环补受影响的独立复核与最终验证。 [D10-T03 验收](../2026-11-26-add-forward-plus-opaque-path/tasks.md#final-verification) · [构建说明](../../../../Docs/Build.md) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md) · [审查闭环](../../../../AGENTS.md#review-loop)

<a id="a-local-verification"></a>
## A 本地验证与交接

A 侧实现及本地检查完成。基准为 `v2.0` 的 `e1c5f85f96daaa3be043520416caa24d12e9e50f`。交接清单逐文件登记 16 个产品改动、7 个合同输入、46 个产品 Shader 包、原生探针链接输入及日志。清单 SHA-256 为 `df00a06b04fc57a653d4b795437da79ed84713270f36cadd44bf9f7f6c0f929a`。 [交接清单](../../../../build/d10-t04/a/handoff.json) · [签收问题单](../../../../build/d10-t04/final/review-signed.md)

| 检查 | 实际结果 | 记录 |
| --- | --- | --- |
| V1 编辑及文档 | 从 EditMainView 提交路径与 FXAA，复核同值幂等、撤销重做及 Open 冻结。旧 v1 缺省 Forward，保存后重开恢复 Forward+ 请求，有效路径等待正常确认。显式 null 的错误定位到 `/mainView/requested/opaquePath`，原场景、文档路径和请求继续有效。实际 ImGui 文本同时显示 Forward+ 请求和有效路径尚未确认。 | [文档回放](../../../../build/d10-t04/a/documents-final.log) · [面板文本](../../../../build/d10-t04/a/view-settings.log) |
| V2 请求取代和双 View | 每帧一个原生键的额度下，待选 Forward+ 时 Forward 以移动后的相机继续提交，票据由 19 增至 20 并接续兼容历史。往返请求后主 View 在票据 22 确认 None 配置下的路径切换。FXAA 切换为票据 30，延迟 GPU 结果保留原 Forward 票据 29。离屏 View 在同一 Scene 运行独立模式和布局。 | [正常帧回放](../../../../build/d10-t04/a/native-final.log) |
| V3 Shader 错误 | 在开发副本中让 `StaticForwardPlusPhysical` 的 fragment 实际编译失败。受光主 View 以 Forward 继续提交，离屏 Direct Lighting View 独立接受 Forward+，同批票据为 21。关闭离屏后核对主路实际 View 上传未包含未选的 Froxel 组。修正 Shader 后当前请求在票据 85 接受。 | [编译失败与恢复](../../../../build/d10-t04/a/shader-final.log) |
| V4 提交、历史及保活 | 等待期间离屏在票据 23 接受 resize，随后在票据 24 确认新路径，两次变化均重置受影响的历史资格。保留旧图后复读 7,857 个显示像素逐字节相同，解除读取引用后池的保留占用下降。主视口无采样及池拒绝均保留原确认。真实绘制后的一次普通 Validation Error 产生票据 35，图标记未接受，旧路径和场景记录继续有效。共享驻留与 Scene 身份在切换前后吻合。 | [原生提交回放](../../../../build/d10-t04/a/native-final.log) |
| 受影响的既有回归 | 用本轮 Release 库重链 D10-T03 的 HDR/D32、图权限及 Froxel 上传夹具。颜色无消费时图中相关 Pass 和写入均为零。参数块限额差一字节时创建零缓冲，取消就绪的候选时撤回该路写入及预留。旧上传夹具在签收基线也报同一失败，比较起点已包含该路的预留。本轮按单主路的额度检验撤回，并检验预留转为四笔实际写入时总额守恒。 | [HDR/D32 回归](../../../../build/d10-t04/a/hdr-final.log) · [现行上传检查](../../../../build/d10-t04/a/uploads-final.log) · [原夹具基线对照](../../../../build/d10-t04/a/legacy-uploads-baseline.log) |

Debug、Release 均构建 `Physara`，两份产品用 `--hidden --poll-limit=15` 启动并正常退出。五项原生回放使用 Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile 及 OpenGL 4.6.0 驱动 32.0.101.6129。真实面板绘制和会话事务已检查，鼠标手势自动化与其它驱动未覆盖。时域纹理的重投影和累积由相应效果工作包验收。 [Debug 构建](../../../../build/d10-t04/a/build-Debug-final.log) · [Release 构建](../../../../build/d10-t04/a/build-Release-final.log) · [回放结果](../../../../build/d10-t04/a/replay-results.json)

从工作区根目录运行 `./build/d10-t04/a/CompileProbe.ps1 -Probe SwitchProbe -Files @('build/d10-t04/a/SwitchProbe.cpp','build/Runtime/Physara.dir/Release/Frame.obj','build/Runtime/Physara.dir/Release/FrameStatistics.obj')` 重链。用生成的 `SwitchProbe-Release.exe` 分别运行 `documents`、`native`、`shader`、`hdr` 和 `uploads`。GLSL 编译错误和普通 Validation Error 均由开发夹具触发。

<a id="a-correction-1"></a>
## 第 1 轮 A 整改与自检

B 的第一轮审查及注释已纳入整改工作树。版本以 `e1c5f85f96daaa3be043520416caa24d12e9e50f` 为基准，20 个产品改动及实际链接输入以清单为准。清单 SHA-256 为 `3391e18c6bf01518da75f6d75fe8149fe47c7b4525ee989538285ad3f8738ca0`。 [待复核清单](../../../../build/d10-t04/a-correction-1/handoff.json) · [逐项问题单](../../../../build/d10-t04/final/review-signed.md)

| 问题 | 整改后的原生对照 |
| --- | --- |
| CR-001 | 局部 Froxel headers 分配 Error 与有效策略收尾各有原生诊断区间。连续 5 次注入期间 Forward 从票据 27 推进至 32，使用当次相机。解除故障后 Forward+ 确认于 33。 [分配回放](../../../../build/d10-t04/a-correction-1/allocation.log) |
| CR-002 | Pass 拒绝绑定实际候选输入。首次失败形成未接受的前缀票据，其后相同条件的 4 帧只绘制仍有效的 Forward。相机输入变化可再次尝试，重发合法的新意图后正常确认。 [拒绝回放](../../../../build/d10-t04/a-correction-1/draw.log) |
| CR-003 | 用相同的 2,294,460 B 驻留余量核对有无主路切换。离屏正常提交 31、38，额外需求等待。释放占用后两路可同时接受。 [驻留回放](../../../../build/d10-t04/a-correction-1/residency.log) |
| CR-004 | 主路绘制失败归属实际 Forward+ 私有 Pass。共同图失败保留两路旧事实，离屏候选保留自己的准备状态，首个独立健康帧直接确认 Forward。 [双路回放](../../../../build/d10-t04/a-correction-1/two.log) |
| CR-005 | 同相机连续位姿变化接续旧记录，显式 cut 在有效帧前一直待确认。票据 24 获接受时前序为零，后继接受帧接续 24。 [相机回放](../../../../build/d10-t04/a-correction-1/cut.log) |
| CR-006 | 同一 Scene 的受光预览与实际接受测试图共享最近的有效路径。仅测试图显示路径适用范围提示。 [面板回放](../../../../build/d10-t04/a-correction-1/ui.log) |

两配置产品均重新构建并以 `--hidden --poll-limit=15` 正常退出。在 Windows x64、MSVC 19.44.35228 与 Intel OpenGL 4.6.0 驱动 32.0.101.6129 上，6 项定向回放和 5 项既有回归均通过。回归包括文档、正常帧身份与池压力恢复、Shader 编译失败、HDR/D32 及上传撤回。Fatal 沿实际宿主退出分支保留，本轮注入覆盖普通错误。 [Debug 构建](../../../../build/d10-t04/a-correction-1/build-Debug.log) · [Release 构建](../../../../build/d10-t04/a-correction-1/build-Release.log) · [回放结果](../../../../build/d10-t04/a-correction-1/replay-results.json)

从工作区根目录调用 `./build/d10-t04/a-correction-1/CompileProbe.ps1 -Probe Review -Files @('build/d10-t04/a-correction-1/Review.cpp','build/Runtime/Physara.dir/Release/Frame.obj','build/Runtime/Physara.dir/Release/FrameStatistics.obj')` 重链。按结果文件的模式运行 `Review-Release.exe`。开发探针沿用 B 场景及 A 已验证的共享夹具。本轮待 B 独立复核。

<a id="b-recheck-1"></a>
## 第一次 B 整改复核

B 核对 A 整改清单的 20 个产品文件、7 个合同、25 个原生链接输入和 46 个产品 Shader 包。在同一 Intel OpenGL 4.6.0 设备上独立重链回放，6 个原定向用例均通过。CR-001、004、005、006 已由 B 勾选通过。8 个产品文件补充了诊断区间、拒绝输入、相机 cut 和 Pass 归属等注释。[B 输入与产物](../../../../build/d10-t04/b-recheck-1/handoff.json) · [逐项结论与注释覆盖](../../../../build/d10-t04/final/review-signed.md)

- **CR-002 待复修。** 已有有效路径时，候选的私有 Froxel 上传失败未登记到所属 View。首次受光时，即使着色 Pass 已正确归因，也未保存失败输入。两者分别在固定输入下连续 4 帧重试，离屏均未收到新票据。[私有上传](../../../../build/d10-t04/b-recheck-1/private-upload.log) · [首次受光失败](../../../../build/d10-t04/b-recheck-1/first-rejection.log)
- **CR-003 待复修。** 两种首次受光请求在相同的 3,736,252 字节驻留余量下比较。主视口首次请求 Forward 时离屏 Forward+ 正常提交；首次请求 Forward+ 时主路抢先得到额度，离屏收到零票据。[Forward 对照](../../../../build/d10-t04/b-recheck-1/first-full.log) · [Forward+ 对照](../../../../build/d10-t04/b-recheck-1/first-plus.log)

第 5.3 项保留待办，下一轮复核上述两项和修正影响的注释。产品构建、启动和其余既有回归引用 A 整改记录，本次未触发 Fatal 或跨驱动验证。

<a id="a-correction-2"></a>
## 第 2 轮 A 整改与自检

保留 B 首次复核的原有结论，本轮复修 CR-002 和 CR-003。基准仍为 `e1c5f85f96daaa3be043520416caa24d12e9e50f`。20 个产品文件和原生夹具、链接输入、46 个 Shader 包均登记在清单中，SHA-256 为 `6c228ce7aaa6a70db0266a23d521917258ed463c49ec23cee7d84d853bad078a`。 [第 2 轮待复核清单](../../../../build/d10-t04/a-correction-2/handoff.json) · [问题单](../../../../build/d10-t04/final/review-signed.md)

| 问题 | 改动及实际结果 |
| --- | --- |
| CR-002 私有上传归属 | Graph Pass 保留来源私有上传的 View 身份。首次注入只形成保活票据 25，随后固定输入的 3 帧由两路原有效策略提交 26–28，Froxel 上传故障仍只触发一次。变更相机后收到新路径的票据 29。 [私有上传回放](../../../../build/d10-t04/a-correction-2/private-upload.log) |
| CR-002 首次受光拒绝 | 第一次绘制失败保存实际候选输入和首个失败 Pass。后三帧继续显示可读材质预览，离屏接受 8–10。更新相机输入后，两路均接受票据 11。 [首次受光回放](../../../../build/d10-t04/a-correction-2/first-rejection.log) |
| CR-003 首次受光准入 | 首次计划延后申请 Froxel 写入额度，生效的离屏方案优先取得所需的驻留和写入。相同的 3,736,252 B 余量下，主路首次请求 Forward 或 Forward+ 时，离屏均提交 19。主路 Forward+ 等待容量，释放占用后在 20 确认。已有路径间切换的 2,294,460 B 对照继续通过。 [首次 Forward](../../../../build/d10-t04/a-correction-2/first-full.log) · [首次 Forward+](../../../../build/d10-t04/a-correction-2/first-plus.log) · [已生效路径间切换](../../../../build/d10-t04/a-correction-2/residency.log) |

Debug 与 Release 产品均重建并以 `--hidden --poll-limit=15` 正常退出。Intel RaptorLake-S Mobile 与 OpenGL 4.6.0 驱动 32.0.101.6129 上，四项补充模式及七项针对性回归通过。上传归属回归按实际写入目标判定，早期夹具把分类为 `ForwardStatic` 的 View 独占灯表也视为共享写入，校正后用同一 Release 产物重链。原夹具输出保留在清单中。 [Debug 构建](../../../../build/d10-t04/a-correction-2/build-Debug.log) · [Release 构建](../../../../build/d10-t04/a-correction-2/build-Release.log) · [上传归属对照](../../../../build/d10-t04/a-correction-2/upload-ownership.log) · [回放模式与结果](../../../../build/d10-t04/a-correction-2/verified-replays.json)

在工作区根目录运行 `./build/d10-t04/a-correction-2/CompileProbe.ps1 -Probe Review -Files @('build/d10-t04/a-correction-2/Review.cpp','build/Runtime/Physara.dir/Release/Frame.obj','build/Runtime/Physara.dir/Release/FrameStatistics.obj')` 重链主探针。上传归属用同一脚本和链接输入，探针名为 `UploadOwnership`，入口为 `UploadOwnership.cpp`。本轮保持待 B 独立复核。

<a id="b-recheck-2"></a>
## 第二次 B 整改复核与签收

**B 签收 D10-T04。** 基准为 `v2.0` 的 `e1c5f85f96daaa3be043520416caa24d12e9e50f` 加本次签收清单列出的 20 个产品文件。B 核对 7 个合同、25 个链接输入、46 个 Shader 包和 A 的构建产物。此次补充的 5 处均为独立注释行，逐处还原后全部产品文件与 A 第二次整改清单逐字节匹配。B 签收清单 SHA-256 为 `ba26fcb0cc8b4469f0b8f971c63e9674b780a67c4aa63b3e71ecac5a75d18ce0`。[签收源码、产物与回放清单](../../../../build/d10-t04/b-recheck-2/handoff.json) · [六项复核与注释覆盖](../../../../build/d10-t04/final/review-signed.md)

| 复核范围 | B 在签收版本上的结果 |
|---|---|
| CR-002，私有上传与首次受光失败 | 主路和离屏分别注入私有 Froxel 上传错误，均只触发一次，失败提交保留前缀，两路随后继续接受原策略。首次受光失败时主路保留原材质预览，离屏连续获得新票据。对应输入改变后候选恢复。另用已核对身份的归属探针确认公共页、独占灯表与 View 常量的归属。[主路上传](../../../../build/d10-t04/b-recheck-2/private-upload.log) · [离屏上传](../../../../build/d10-t04/b-recheck-2/offscreen-upload.log) · [首次受光](../../../../build/d10-t04/b-recheck-2/first-rejection.log) · [上传归属](../../../../build/d10-t04/b-recheck-2/upload-ownership.log) |
| CR-003，驻留额度与双路连续出图 | 相同余量 3,736,252 B 下，主路首次请求 Forward 或 Forward+ 时，已有效的离屏 Forward+ 均取得票据 19。仅新增的主路 Forward+ 等待驻留，释放占用后在 20 确认。原单组余量下的既有路径切换亦通过。[首次 Forward](../../../../build/d10-t04/b-recheck-2/first-full.log) · [首次 Forward+](../../../../build/d10-t04/b-recheck-2/first-plus.log) · [原驻留对照](../../../../build/d10-t04/b-recheck-2/residency.log) |
| 其它受影响入口 | 候选分配连续失败时有效 Forward 继续出图，私有绘制失败只归对应 View，同输入不重复尝试。显式相机 cut 与测试图范围提示均通过。[分配](../../../../build/d10-t04/b-recheck-2/allocation.log) · [双路归因](../../../../build/d10-t04/b-recheck-2/two.log) · [绘制拒绝](../../../../build/d10-t04/b-recheck-2/draw.log) · [相机](../../../../build/d10-t04/b-recheck-2/cut.log) · [面板](../../../../build/d10-t04/b-recheck-2/ui.log) |

B 在 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129 上运行上述 12 项回放。Debug/Release 产品构建与启动、正常帧和 Shader 源错误回归引用 A 的同输入记录。Fatal 与其它驱动未作原生回放。第 6 节由 A 对齐本次签收输入后执行。

<a id="final-verification"></a>
## A 最终验证

已核对 B 签收时的 126 个输入，包括当前 20 个产品文件和 12 项 B 回放的输入及结果。记录的 5 个注释文件纳入最终构建。Debug、Release 均从签收源码构建并以 `--hidden --poll-limit=15` 正常退出，两配置共 46 个 Shader 包与签收摘要一致。已保留签收问题单的原始字节。 [签收输入](../../../../build/d10-t04/final/signed-inputs.json) · [签收问题单](../../../../build/d10-t04/final/review-signed.md) · [Debug 构建](../../../../build/d10-t04/final/build-Debug.log) · [Release 构建](../../../../build/d10-t04/final/build-Release.log)

| 规格合同 | 实现及签收验证 |
| --- | --- |
| 每 View 的不透明路径选择与确认 | View 记录请求和有效策略，Runtime 用实际采样或离屏导出和正常提交确认各自路径。最终原生回放覆盖等待中继续绘制、相机移动、往返请求、None/FXAA 与延迟 GPU 身份。Shader 失败时有效 Forward 连续出图，修复后 Forward+ 确认于票据 93。 [正常帧](../../../../build/d10-t04/final/native.log) · [Shader 故障与恢复](../../../../build/d10-t04/final/shader.log) |
| 私有上传归属、驻留与历史资格 | 离屏私有上传只失败一次，失败前缀用于保活，两路随后在票据 26–28 正常提交，变化后于 29 恢复。首次受光失败保留主路旧材质预览，离屏获得 8–10，输入变化后于 11 恢复。首次 Forward+ 等待时离屏继续接受 19，释放驻留额度后两路在 20 确认。主路私有上传、有效路径等待、显式 cut 和共享／私有上传归属沿用身份核对通过的 B 回放。 [离屏上传](../../../../build/d10-t04/final/offscreen-upload.log) · [首次受光](../../../../build/d10-t04/final/first-rejection.log) · [首条路径额度](../../../../build/d10-t04/final/first-plus.log) · [B 签收验证](#b-recheck-2) |
| 编辑及 v1 文档恢复 | View Settings 经 EditorSession 编辑当前主 View 的权威请求，实际有效值来自接受记录。v1 回放确认旧文件缺省 Forward、合法请求的 Undo/Redo 和保存重开、冻结编辑及非法字段拒绝。实际受光／测试图的适用范围提示引用 B 面板对照。 [文档回放](../../../../build/d10-t04/final/documents.log) · [B 面板对照](../../../../build/d10-t04/b-recheck-2/ui.log) |

最终核对 3 个增量能力的 4 条要求和 21 个场景，主要风险按上述实现入口和验收证据核实。签收版重链实际运行 6 项原生回放，另外复用 9 项输入摘要匹配的 B 回放。设备为 Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 32.0.101.6129。普通 RHI Error 由开发夹具触发，Fatal 退出沿已审合同核查，本轮原生覆盖限于该驱动和故障范围。 [构建、链接与回放摘要](../../../../build/d10-t04/final/native-verify.json) · [同步前变更校验](../../../../build/d10-t04/final/pre-sync-validation.json)

<a id="spec-sync"></a>
## 规格同步

`editor-domain-edits` 增加主 View 路径编辑要求。`scene-document-persistence` 更新主 View 请求持久化要求。`view-preparation` 更新每 View 路径确认要求并增加历史兼容性要求。同步后逐项核对 21 个增量场景和未涉及的要求，变更和 43 份主规格均通过严格校验。 [同步记录](../../../../build/d10-t04/final/spec-sync.json) · [主规格校验](../../../../build/d10-t04/final/post-sync-main-validation.json)

<a id="archive-closeout"></a>
## 归档与阶段推进

D10-T04 已验收并归档，下一工作包为 D10-T05。B 的六项签收结论、最终验证及主规格同步记录均已留存。签收问题单保留原字节，原文链接以 Docs 为基准。 [签收问题单](../../../../build/d10-t04/final/review-signed.md) · [当前阶段](../../../../Docs/Developments/Developments.md#current-stage) · [下一工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05)
