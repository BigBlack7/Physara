# Tasks

## 1. Scene 变更与安全槽提取

- [x] 1.1 A 在已提交 Scene 事务中产出结构代和按实际影响归并的变更，接到持续输入边界，覆盖父级传播、覆盖源槽、灯、环境与有效资产切换。用 V1 核对无可用槽时的编辑合并、撤销重做及同值操作。 [Scene 规范](specs/static-scene/spec.md) · [变更发布设计](design.md#committed-scene-changes)
- [x] 1.2 A 在 FrameSlot 按 Scene 身份、结构代和版本更新持久记录、索引、固定资产引用和聚合 bounds。用 V1 核对有界脏集合、稀疏改写、极值收缩、结构换代、空记录及旧槽借用的隔离。 [快照规范](specs/scene-frame-preparation/spec.md) · [槽存储设计](design.md#slot-scene-storage)

## 2. 静态编码与 View 数据

- [x] 2.1 A 把对象与材质的实际依赖核对放在编码前，按固定源及实例覆盖代复用驻留。分开灯世界行、各 View 的灯表和 View 常量。用 V2 证明暖机后零静态重打包、相机／曝光独立更新、单项覆盖与灯修改只使实际依赖失效，当前 Forward 结果保持匹配。 [驻留规范](specs/static-gpu-residency/spec.md) · [编码设计](design.md#static-encodings)

## 3. 同图多 View 和输出确认

- [x] 3.1 A 让 Runtime 正常帧入口接收有界离屏请求集合，合法的各路使用各自 View 工作和一次共享快照。用 V3 核对独立索引、相机及尺寸，用 V4 核对一项无效请求、容量拒绝及全部请求不可准备时零提取。 [View 规范](specs/view-preparation/spec.md)
- [x] 3.2 A 让同图的各 View 共用匹配的上传和资源版本，统一实际纹理及缓冲声明顺序。用 V3 核对冷帧共享写入与图依赖，用 V4 核对被裁剪的私有写入和其他存活共享生产者。 [驻留规范](specs/static-gpu-residency/spec.md)
- [x] 3.3 A 接通主视口采样与附加离屏显式导出，并按各 View 的实际生产、请求和同图提交回执确认。用 V3 原生读回核对两路独立输出。V4 核对局部请求失败、主图裁剪或失败时的有效输出及资源保活。 [帧图规范](specs/editor-frame-graph/spec.md) · [多 View 设计](design.md#offscreen-view-outputs)

## 4. 统计与诊断

- [x] 4.1 A 在现有统计通道记录共享提取／各 View 裁剪、实际改写及编码、静态／动态上传和两个 arena 的高水位与扩容。用 V2–V4 的同一回放按槽、View 与提交身份核对计数、正常发布及恢复时保留的快照。 [统计规范](specs/render-frame-statistics/spec.md) · [统计设计](design.md#preparation-statistics)

## 5. A 本地集成与交接

- [x] 5.1 A 构建实际受影响的 Physara 消费者，完成 V1–V4 的局部与集成检查，运行 change 严格校验，记录实际源码、构建、测试及未覆盖范围。在现行问题单登记基准 commit、相关 diff、新增文件与可复现输入，交维护者调度独立 B。 [验证范围](#verification-scope) · [B 签收原件](../../../../build/d08-t01/final/review-signed.md)

## 6. 独立审查、整改和复核

- [x] 6.1 B 独立核对实审版本、变更路径和实际消费者，重点复查脏集合与槽寿命、同图生产与回执、输出及统计归属。按项目标准检查并完善本轮涉及的完整第一方文件注释。记录覆盖、正确性与注释结论以及编号问题。 [B 签收原件](../../../../build/d08-t01/final/review-signed.md) · [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 6.2 A 按编号整改并自检受影响合同，保留对应版本、回归及限制，提交 B 复核。 [第 1 轮 A 整改](#a-correction-1) · [A 再次整改](#a-correction-2)
- [x] 6.3 B 独立复核最终整改、匹配的回归证据和注释，分别确认代码正确性与注释可读性，关闭正式问题并签收实际最终版本。复核未通过时沿原编号继续交回 A。 [第 1 轮 B 复核](#b-recheck-1) · [B 最终签收](#b-recheck-2)

## 7. 最终验证、同步与归档

- [x] 7.1 A 对 B 签收版执行最终 verify，核对当前正式输入、二进制和资源身份，重放受影响的原生双 View 与回执检查，复用匹配的 CPU 或局部证据。核对静态复用、结构追赶、输出及统计达到 D08-T01 完成条件。 [最终验证](#final-verification) · [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01)
- [x] 7.2 A 在 change 中留存签收版本、CR、注释覆盖、最终证据和限制，使用 OpenSpec 同步六份增量，并核对主规范保留既有要求和场景。 [规范同步](#spec-sync)
- [x] 7.3 A 使用 OpenSpec 归档，核对归档路径、相对链接和任务状态，更新开发计划的 D08-T01 状态与后继工作包入口。归档成功并留存证据后按模板重置问题单。 [归档收尾](#archive-closeout)

<a id="verification-scope"></a>
## 验证范围与停止条件

| 验证 | 风险和最小检查 |
|---|---|
| V1 · CPU 变更、规模与安全槽 | 用固定小网格实例扩成数千对象的 CPU 场景。三个槽均准备后，让一个槽保持不可复用，在多轮提交中移动有灯和网格后代的父级、重复修改一个源槽覆盖并重放，随后使槽以不同顺序追赶。用已知身份、世界位置、覆盖值、聚合范围和记录计数核对局部改写、合并上界与旧槽数据。继续以该输入核对最远对象隐藏或内移、增删、相同内容 revision 的不同 Scene 和空记录。只在槽的合法借用期内检查旧记录及固定资产。 |
| V2 · 现有 Forward 与静态成本 | 复用 `ForwardReference` 和实际帧入口。先让所有槽达到稳态，再冻结输入、只改相机或曝光且保持可见集合，然后分别改一项覆盖和一盏可见灯。核对记录更新、静态编码／实际静态写入为零的稳态，变化时沿实际依赖更新。用选定 HDR 样点和已知曝光变化核对输出，按槽核对 arena 实际扩容及高水位。 |
| V3 · 冷帧双 View | 同一生产帧提交合法主视口和附加离屏请求，使用可区分的相机及输出尺寸。核对一次场景提取、两个独立索引集、确切的图出口及共享资源写入一次。读回两个输出的代表像素，与各自单 View 参考或固定样点预期比较，核对实际接受的 View 身份、尺寸、曝光和归属的同一提交。 |
| V4 · 无效请求、图裁剪与提交资格 | 在 V3 的帧序列中测试附加请求被拒绝、全部请求不可准备，以及主视口失去最后一个消费者而附加路仍出图。核对候选被拒绝或剪除后，存活 View 的共享生产者仍完整。复用现有可恢复帧失败入口覆盖一次已发写入后的前缀提交，核对实际在途引用、未确认写入及保留的 View 与统计。后续合法请求能够重试，基础 UI 与退出沿真实路径检查。 |

优先使用 D07 的固定资产、回放方法及相同输入的适用证据。大场景 CPU 检查与原生双 View 检查分别对照各自的容量与实际消费者。额外的复制帧或故障注入仅用于识别现有入口无法区分的回归。新增开发脚本、样例和临时工程只存 `build/d08-t01/`。实际检查记录应标识源码与构建、资源身份、设备驱动、使用的 View 和内容版本，说明复用证据及未覆盖路径。 [基线参考场景](../../../../Assets/README.md#forward-参考场景) · [D07 原生记录](../2026-11-19-add-physical-scene-authoring/tasks.md#final-verification)

验证到能证明本 change 的增量计数、输出、寿命和回执合同即停止。发生输入变化、失败或出现具体证据缺口时扩大对应检查。若需报告耗时收益，按架构使用相同场景、相机、画质、设备、预热和采样口径记录 CPU、GPU 和等待，阶段性比较由 D08-T08 收口。 [测量合同](../../../../Docs/Architecture.md#benchmark) · [验证技能](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-checks"></a>
## A 本地证据

- V1 已在 Windows x64 的正式 Release Scene、Renderer 和 Core 库上通过。4,096 个对象、8,192 个源 Primitive 槽，24 轮共 480 次局部编辑加撤销重做。每轮合并结果为对象变换、灯及源槽覆盖三条记录，各槽按不同顺序追赶。检查包括旧槽数据、零改写复用、极值收缩、结构增删、固定模型代并存、同版号换 Scene 及空场景。槽完成时序由 CPU 样例控制，原生提交和读取在 V2–V4 验证。 [入口](../../../../build/d08-t01/Verify.ps1) · [V1 结果](../../../../build/d08-t01/SceneSlots.log)
- V2 用 `ForwardReference` 的三个真实槽确认静态暖机后的零改写、零重编和零静态写入。只改附加 View 的曝光时静态写入为零，受光样点 0.0793457 对应 0.158691，自发光 2.5 对应 5。材质编辑触发一次编码和一次静态写入，灯编辑只编一次世界灯行。已接受快照的槽、裁剪和 arena 峰值、容量及扩容计数通过同一回放核对。
- V3 在同一设备上比较匹配冷帧的单路和双路。静态写入为 80 和 81 笔，字节为 18,056,768 和 19,105,344，差值 1,048,576 字节，等于附加 View 私有灯表。两路借用同一次冷提取，分别接受 664×500 和 320×240 的离屏输出。主路与单路对照、附加路与仅导出自身时的对照按原生显示像素比较，改变附加相机独立影响该路可见集。
- V4 核对陈旧身份、超容量、缺布局和主视口实际 Image 被全裁剪。附加出口仍存活，保留的主 View 颜色代和统计中的接受资格一致。在真实录制已发写入后，运行 build 内的图 Pass 故障入口，前缀得到票据 1809，View 及统计保持上一次有效结果。票据 1810 重新写入未确认数据并正常出图。

V2–V4 使用当次正式 Release 库及 Shader 包，复用 D07 的固定场景输入和已定位的曝光见证点。本地回放的 Runtime 副本只有三处观察或故障入口，产物留在 build。实机证据限 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 的 Windows OpenGL 宿主。 [原生回放](../../../../build/d08-t01/Verify.ps1) · [原生结果](../../../../build/d08-t01/Native.log)

以 Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0 构建当前正式 `Physara` 的 Debug 和 Release，两个产品经 `--hidden --poll-limit=5` 正常退出。OpenSpec 1.13.2 严格校验通过。A 的检查结果用于独立审查交接，最终 verify 使用 B 签收的输入执行。 [Debug 构建](../../../../build/d08-t01/debug-final-build.log) · [Release 构建](../../../../build/d08-t01/release-final-build.log) · [严格校验](../../../../build/d08-t01/validate-change.json) · [待审输入与产物](../../../../build/d08-t01/handoff.json)

<a id="a-correction-1"></a>
## 第 1 轮 A 整改

整改前核对了 B 注释交接版的源码 diff SHA-256，保留 B 的审查记录和注释。逐项整改与回归结果登记在同一问题单。 [B 签收原件](../../../../build/d08-t01/final/review-signed.md) · [B 初审快照](../../../../build/d08-t01/a-correction-1/b-review.md)

| 问题 | 定向结果 |
|---|---|
| CR-001 | CPU 的同源交替请求在 40 轮中有 39 轮就绪。换源时确认新产品的辐亮度，随后重选旧源仍需准备。真实宿主上冷天空物理 View 和 BaseColor View 同票据获得独立输出。 |
| CR-002 | 两路各有两张独有 2048×2048 纹理时，附加路单独出图与主路全裁剪时均实际写入 12 笔、34,604,404 字节，读回匹配，主路继续保有旧颜色代。V3 的冷帧共用写入仍只增加附加路的一张私有灯表。 |
| CR-003 | 1024 设备纹理上限和 1536×1024 显示画布的离屏请求返回尺寸拒绝，工作集为空，共享提取为零。缩小后可正常准备，主视口空文档诊断图仍使用渲染栅格。 |
| CR-004 | 视锥外增删无灯网格后的三个槽均为零灯编码、零灯表写入。可见灯变更及灯实体在原紧凑下标换代时，各自只编码一行并写入两张参与消费的灯表。 |
| CR-005 | ImGui 捕获的纯附加路统计按当前 Scene 标为 `Accepted`，在另一 Scene 下标为 `Previous`，两次标签均保留实际 View 和票据。 |

使用当前源码重建 Debug 和 Release，两个正式产品经 `--hidden --poll-limit=5` 正常退出。定向 CPU 与原生样例重链接 Release 库。V2–V4 复用原有 `ForwardReference`、回放和观察入口，在整改版重新验证静态复用、双路出图与失败前缀。V1 仍引用本包已有 CPU 证据。原生结果对应 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129，作为 A 整改自检交 B 复核。 [复现命令](../../../../build/d08-t01/a-correction-1/Verify.ps1) · [CPU 定向结果](../../../../build/d08-t01/a-correction-1/ReviewCpu.log) · [原生定向结果](../../../../build/d08-t01/a-correction-1/ReviewNative.log) · [V2–V4](../../../../build/d08-t01/a-correction-1/Integration.log) · [整改版清单](../../../../build/d08-t01/a-correction-1/handoff.json)

<a id="b-recheck-1"></a>
## 第 1 轮 B 复核

**整包待 A 继续整改。** CR-001、CR-002、CR-004、CR-005 已由 B 关闭。CR-003 保留原编号，新增 CR-006。两项均已在本轮真实 OpenGL 运行中复现。[B 签收原件](../../../../build/d08-t01/final/review-signed.md)

| 编号 | B 复核结果 |
|---|---|
| CR-001、CR-002 | 同源天空与诊断路交错的 CPU 检查就绪 39/40 轮，换源拒收迟到结果。原生物理与 BaseColor 双路在票据 2 同时出图。附加路单独出图和主图全裁剪时均写 12 笔、34,604,404 字节，读回一致。 |
| CR-004、CR-005 | 视锥外网格增删后三槽的灯行编码及灯表写入均为零，灯数据或实体身份换代触发一行编码和两表写入。当前场景与旧文档的 ImGui 标签正确区分同一附加 View 和票据。 |
| CR-003 | 设备单边尺寸拒绝用例通过。B 另测 4096×4096 的唯一离屏请求，附件所需 256 MiB，超过 128 MiB 上限。拒绝后再安全取得同一冷槽，首次观测为 `Reused`，表明已发生提取。单边超限对照为 `Rebuilt`。 |
| CR-006 | 主路从 664×500 改为 884×565 后全裁剪，附加路票据 5 正常，主路仍持旧尺寸和颜色代 1，设置却为 `Applied`。解除裁剪后才接受新尺寸并切换到代 2。 |

B 复核输入的源码 diff SHA-256 为 `b5ae749d84cc516ffbcca71b02c788570aba4c7c2d2a92396fa8bf795704319f`。33 份 C++、2 份变动项目文档、10 份 change 文件，以及清单列出的产物与回放文件全部匹配。B 重跑了整改版 CPU 和原生定向样例。V2–V4 的整帧回放复用 A 整改版证据，V1 沿用既有槽轮转记录。原生设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0，驱动 32.0.101.6129。[整改输入](../../../../build/d08-t01/a-correction-1/handoff.json) · [定向原生结果](../../../../build/d08-t01/a-correction-1/ReviewNative.log) · [B 两项复现及对照](../../../../build/d08-t01/b-recheck-1/Results.log)

B 在 9 份文件补订了环境需求寿命、主视口采样预检、灯表身份和显示色确认等注释。33 份 C++ 去除独立行注释后的摘要与复核输入逐一相同。返给 A 的源码 diff SHA-256 为 `aa0011f4b4c5ced7b9e90ec872342a0efa4d2ddc0a630ad27b9e4bfe39ed3ea5`。6.3 保持待验收，A 的最终 verify、同步及归档仍待签收。[B 签收原件](../../../../build/d08-t01/final/review-signed.md)

<a id="a-correction-2"></a>
## 第 1 轮 A 再次整改

核对了 B 注释交接版的源码 diff SHA-256，沿用本轮 CR-003 和 CR-006 的编号与复现，保留已关闭条目的结论。 [B 复核快照](../../../../build/d08-t01/a-correction-2/b-review.md) · [B 签收原件](../../../../build/d08-t01/final/review-signed.md)

| 编号 | A 定向结果 |
|---|---|
| CR-003 | 离屏预检与 Forward 共享附件预算检查。16385×32 单边超限、4096×4096 固定字节超限，以及 2304×2304 渲染和 2560×2560 显示叠加 FXAA 后的超限请求都获零票据，下一次安全取得对应冷槽时均为 `Rebuilt`。CPU 对照核对较小离屏请求与空文档主视口诊断图。 |
| CR-006 | 664×500 的已接受颜色代 1 在主视口改为 884×565 并全裁剪后仍可读取，设置为 `ViewportSamplingPending`，主路私有上传为零。附加 View 保持实际出图。恢复采样后接受 884×565、颜色代 2，设置为 `Applied`。 |

以当前 Release 库和 Runtime 源码重链接 B 的两项原生复现，另运行 CPU 诊断图对照。Debug 与 Release 正式构建通过，两版均经 `--hidden --poll-limit=5` 正常退出。原生验证对应 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。结果交独立 B 复核。 [复现入口](../../../../build/d08-t01/a-correction-2/Verify.ps1) · [预算](../../../../build/d08-t01/a-correction-2/ExtentBudget.log) · [裁剪](../../../../build/d08-t01/a-correction-2/ClippedLayout.log) · [CPU 对照](../../../../build/d08-t01/a-correction-2/ReviewCpu.log) · [Debug 构建](../../../../build/d08-t01/a-correction-2/debug-build.log) · [Release 构建](../../../../build/d08-t01/a-correction-2/release-build.log) · [严格校验](../../../../build/d08-t01/a-correction-2/validate-change.json) · [待复核清单](../../../../build/d08-t01/a-correction-2/handoff.json)

<a id="b-recheck-2"></a>
## 第 1 轮 B 最终签收

**B 已签收 D08-T01 的代码和注释。** 本次关闭 CR-003 和 CR-006，此前已关闭 CR-001、CR-002、CR-004、CR-005，现有 6 项正式问题全部关闭。任务 6.3 完成，7.1–7.3 由 A 接续。[B 签收原件](../../../../build/d08-t01/final/review-signed.md)

| 编号 | B 在再次整改版上的复核 |
|---|---|
| CR-003 | 单边超限的 16385×32、相同栅格的 4096×4096、2304×2304 渲染到 2560×2560 并启用 FXAA，均在离屏请求提取前得到 `SceneAttachmentLimit`，输出票据均为零。同一冷槽再次安全取得后，第一次准备均为 `Rebuilt`。较小合法请求和空文档主视口诊断图的 CPU 对照也通过。 |
| CR-006 | 缩放主面板并全裁剪后，请求为 884×565，设置为 `ViewportSamplingPending`，有效颜色仍为 664×500 的代 1。B 本次附加路票据为 5，主路私有上传为零。恢复采样后才接受 884×565 的代 2，状态为 `Applied`。 |

B 核对了基准 `252c49bd0e6af25da039b0e657be45cc0298530a`、空暂存区、45 份变动工作区文件和整改清单列出的 68 项产物、30 项回放文件。相对上次 B 交回的源码只有 5 份文件有改动。本次重跑与清单匹配的 Release `ExtentBudget`、`ClippedLayout` 和 `ReviewCpu`，退出码均为 0。原生设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0，驱动 32.0.101.6129。V1 的槽轮转与完整 V2–V4 回放沿用此前对应阶段记录。[A 整改清单](../../../../build/d08-t01/a-correction-2/handoff.json) · [B 实际运行输出](../../../../build/d08-t01/b-recheck-2/Results.log) · [既有回归记录](#b-recheck-1)

实跑输入的源码 diff SHA-256 为 `03ab2729e4a185064a8c3cc3cf30053945e785365c8b94d43e09d3a150127ab7`。B 在 `Forward.hpp` 和 `Forward.cpp` 补充了预检返回值、FXAA 与重采样中间目标、渲染与显示画布的计费说明。33 份第一方 C++ 的非注释行与实跑输入逐一一致。含注释的签收源码 diff SHA-256 为 `76df0c076286d4b9f5ce4edd0eec25d389a61e2a6c839e7064abb300d449c890`，取自 `git -c core.safecrlf=false diff --no-color --no-ext-diff --no-textconv --full-index HEAD -- Editor Engine Runtime` 的原始输出。

<a id="final-verification"></a>
## 最终验证

B 签收工作区的基准为 `252c49bd0e6af25da039b0e657be45cc0298530a`，33 份第一方 C++ 的源码差异与 B 签收摘要一致。CR-001 至 CR-006 均已关闭。B 核对了 Scene 事务及帧槽索引、Forward 与显示附件、环境及灯表、Runtime 图与回执、UI 采样与统计、Core arena 的职责、失败状态和主要流程。保留原始问题单及源码、正式输入、二进制和回放的摘要。 [B 签收原件](../../../../build/d08-t01/final/review-signed.md) · [签收输入](../../../../build/d08-t01/final/signed-source.json) · [最终验收清单](../../../../build/d08-t01/final/verification-evidence.json)

以 Windows x64、MSVC 19.44.35228 和 SDK 10.0.26100.0 重新构建签收版 Debug 与 Release，两版均经 `--hidden --poll-limit=5` 正常退出。四个回放程序重新链接当前 Release 库，核对了 Shader 包、固定参考资产、回放源码及实际运行的二进制摘要。OpenSpec 1.13.2 严格校验通过。 [Debug 构建](../../../../build/d08-t01/final/debug-build.log) · [Release 构建](../../../../build/d08-t01/final/release-build.log) · [严格校验](../../../../build/d08-t01/final/validate-change.json)

| 验收范围 | 签收版结果 |
|---|---|
| Scene 事务与安全槽 | V1 用 4,096 个对象、8,192 个 Primitive 槽和 480 次局部编辑及重放验证三个槽的增量追赶、旧槽保活、结构重建及空场景。 |
| 静态编码、上传与 View 统计 | V2 暖机与曝光变化时静态写入均为零，材质变更编码并写入一次。两处 HDR 样点分别从 0.0793457 到 0.158691、从 2.5 到 5。核对各槽提取及 arena 事实、各路裁剪和来源票据。 |
| 共享上传、双路出图与失败回执 | V3 冷帧的单路和双路分别写 80、81 笔，同图两路读回通过，差值为一张私有灯表。V4 的票据 2810 保活失败前缀并保留原接受状态，重试获票据 2811。 |
| 离屏预检与裁剪等待 | 复跑 CR-003 的三种超限请求，拒绝后安全重取各冷槽的首次提取均为 `Rebuilt`。CR-006 全裁剪期间保留 664×500 的颜色代 1 并等待采样，主路私有上传为零。恢复后接受 884×565 的颜色代 2。 |

七项增量要求与当前事务入口、单槽提取、各路 View、Forward 版本和帧图回执、只读统计实现一致。回放覆盖主要场景，沿用 B 对其余已关闭问题的定向证据。原生验证覆盖 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。 [回放入口](../../../../build/d08-t01/final/Verify.ps1) · [V1](../../../../build/d08-t01/final/SceneSlots.log) · [V2–V4](../../../../build/d08-t01/final/Integration.log) · [CR-003](../../../../build/d08-t01/final/ExtentBudget.log) · [CR-006](../../../../build/d08-t01/final/ClippedLayout.log)

<a id="spec-sync"></a>
## 规范同步

六份增量已合入 `editor-frame-graph`、`render-frame-statistics`、`scene-frame-preparation`、`static-gpu-residency`、`static-scene` 和 `view-preparation`。同步新增四项、更新三项要求，核对了既有场景和增量场景，其中帧槽跨相机移动与场景替换的场景按增量更新了结果。OpenSpec 严格校验全部 36 份主规范通过，逐项比对确认六份增量均已合入。 [同步记录](../../../../build/d08-t01/final/spec-sync.json) · [主规范校验](../../../../build/d08-t01/final/validate-specs.json)

<a id="archive-closeout"></a>
## 归档收尾

OpenSpec 1.13.2 已在六份规范完成同步后归档本 change。D08-T01 已验收并归档，下一工作包为 D08-T02。B 签收原件及最终验收证据已留存，当前问题单已按模板重置。 [归档回执](../../../../build/d08-t01/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage)
