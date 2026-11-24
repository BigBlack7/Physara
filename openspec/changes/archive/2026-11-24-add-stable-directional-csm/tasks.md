# Tasks

## 1. 每 View 请求和 CPU 级联计划

- [x] 1.1 A 扩充会话阴影请求，校验级数、透视对数权重和过渡比例，经当前 View 身份提交。V1 核对合法请求、Release 越界拒绝与原有请求保留。 [View 规范](specs/view-preparation/spec.md) · [请求设计](design.md#settings-and-splits)
- [x] 1.2 A 实现有限透视／正交分段、过渡 receiver、固定光空间 XY 尺度及 texel 吸附，为每级选择保守 caster 并拟合 Z。V1 核对分割数值、吸附微移、过渡带和画外遮挡候选。 [规划规范](specs/shadow-planning/spec.md) · [拟合设计](design.md#stable-fit-and-allocation)
- [x] 1.3 A 接通成组预算准入、稳定区域与每级深度所需内容代，保留局部灯计划和完整直接光输入。V1 核对有效边长、整组拒绝、相容区域保留与相关依赖的版本变化。 [分配和缓存设计](design.md#stable-fit-and-allocation)

## 2. 阴影 Draw 和图内深度生产

- [x] 2.1 A 通过生成接口接入 CSM 常量和深度程序，准备独立的常规深度 Pipeline，保留静态实例与 MASK 的共享入口。沿正式程序目录、构建包和帧前准备路径交付。V4 核对活动 ABI、原生准备和角色键，V2 核对常规深度与 caster bias。 [几何与偏移设计](design.md#caster-draws-and-depth)
- [x] 2.2 A 在共享上传封存前准备实际投射物，并把各级 caster 批次接入同一 RenderGraph。声明精确 D32 层，完成 Clear/Store、深度写和逻辑区域映射。V2 的逐层读回和上传依赖核对画外 caster、镜像／双面实例、MASK 孔和空 caster 层。 [GPU 规范](specs/shadow-rendering/spec.md) · [图生产设计](design.md#graph-production-and-retention)

## 3. Forward 级联诊断及跨帧接受

- [x] 3.1 A 接入有效层的二维采样视图、稳定线性视深选级和过渡比较。正常 Forward 使用诊断程序与匹配的 ViewOutput。V2 核对实际图的层范围、近远区间及过渡样点，V4 核对诊断程序的采样绑定。 [GPU 规范](specs/shadow-rendering/spec.md) · [诊断设计](design.md#diagnostic-consumer)
- [x] 3.2 A 将只读深度组的保留与匹配 View 的活性、导出及完整接受回执关联。V3 核对只读层组交接、单层采样权限、静态复用、版本失效、拒绝后重试及另一 View 的独立结果。 [保留设计](design.md#graph-production-and-retention)
- [x] 3.3 A 接通现有 Renderer 面板的会话诊断开关、实际接受状态和不可用原因。V3 经面板使用的类型化请求入口核对启停、无有效区域及再次接受后的画面与请求。 [Forward 规范](specs/static-forward-rendering/spec.md)

## 4. A 集成自检与交接

- [x] 4.1 A 以当前输入构建正式受影响消费者并运行 V1–V4。OpenSpec 严格校验通过，交接清单登记工作树、命令、固定场景输入、程序包、二进制和检查范围。已交维护者调度 B。 [A 本地验证](#a-local-verification) · [本轮问题单](../../../../build/d09-t02/final/review-signed.md)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核对计划与实际图消费者，重点复核过渡覆盖、光空间绕序及 bias、层授权与接受版本。按项目约定核对并完善本轮涉及的完整第一方文件的职责、接口和流程注释，在当前问题单登记证据和发现。 [B 首审记录](#b-review-1) · [审查与注释约定](../../../../AGENTS.md#review-loop)
- [x] 5.2 A 按问题单编号整改，针对受影响的可观察结果自检。记录当前正式输入及对应证据，由维护者交回 B 复核。 [A 首轮整改](#a-correction-1) · [A 第 2 轮整改](#a-correction-2) · [A 第 3 轮整改](#a-correction-3) · [A 第 4 轮整改](#a-correction-4)
- [x] 5.3 B 复核实际版本、整改证据及注释。第 4 轮关闭 CR-012，CR-001 至 CR-012 均已通过，B 签收当前产品源码。 [首次复核](#b-recheck-1) · [第 2 轮](#b-recheck-2) · [第 3 轮](#b-recheck-3) · [最终签收](#b-recheck-4)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后对签收版本执行最终 verify。核对正式源码、Shader 包、二进制及检查输入身份，按 D09-T02 完成条件确认 CPU 结果及真实 OpenGL 的层深度、稳定性与遮挡。记录实跑、复用证据及其适用范围。 [最终验证](#final-verification) · [工作包完成条件](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02)
- [x] 6.2 A 在变更中留存 B 签收、正式问题和接受证据，核对规范增量与主规范的同步结果，运行严格校验并检查链接。 [规范同步](#spec-sync)
- [x] 6.3 A 执行 OpenSpec 归档，复核归档位置、文档链接及工作包状态。更新 D09-T02 验收链接和 D09-T03 前置，留存签收记录后复位当前问题单。 [归档验收](#archive-closeout)

<a id="verification-scope"></a>
## 验证口径与停止条件

| 检查 | 具体风险 | 最小可判定结果 |
|---|---|---|
| V1 · CPU 计划 | 划分或吸附不稳、过渡带裁剪、整组超配、内容代污染 | 使用一个合成冻结场景。对照透视四级的 `1, 12, 25, 44, 81` m、零近平面正交四级的 `0, 20, 40, 60, 80` m 和一级端点。用独立的世界样点核对同格与跨一格的 XY，并确认过渡接收点与画外 caster 同时进入需要的级联。给四级 4 MiB，核对 512 档整组准入，收紧为三层则无分配。用相容的前后计划核对区域身份及 caster 编辑引起的所需版本变化。用一次越界请求确认拒绝及保留。 |
| V2 · 正式 OpenGL 深度与查询 | 写错层、使用了主 Reverse-Z／错误绕序、漏掉画外或 MASK 覆盖、只查一级造成跳变 | 使用一份具有已知近远投影与遮挡点的非对称场景，合并 MASK 孔、负缩放、双面、静态实例和画外 caster。对照实际图访问及图内 staging 读回的 D32 各层、主深度和诊断颜色。选择带内至少一个两级比较结果不同的样点，独立读取两层并对照权重和带外结果。以同一场景慢移并切除最后一个 caster，确认稳定 XY、正常深度方向与清除恢复。 |
| V3 · 正式帧发布与复用 | CPU dirty 被误作已生产、仅前缀票据被误作接受、诊断影响另一 View | 在同场景的主 View 与一个离屏 View 使用不同请求，复用 V2 的有效输入。用图记录核对静态跨安全槽无深度重绘、图采样范围和接受身份。修改一项 caster 覆盖，使一次已记录写入的继任候选未获完整接受，随后重试，对照采样所用组及回执。利用同一回放核对禁用或无分配时的可用画面、请求与接受状态。 |
| V4 · 程序与构建 | 生成接口、包依赖或活动绑定失配，镜像／双面或主深度命中错误 Pipeline | 正式 Windows x64 Debug、Release 构建生成并消费新程序。核对实际反射、D32 附件及单层采样槽，与 V2 的真实设备编译、link 和帧执行相互印证。沿现有完整 Shader 代准备检查深度程序换代时资源代选择及 V3 深度失效。 |

优先复用 D09-T01 已登记且仍可用的合成场景与检查入口。旧单方向段的预算回放可显式使用一级请求。开发检查及输入只放在 `build/d09-t02/`，通过正式场景、图和 RHI 入口运行。CPU、包和原生证据各自以实际受影响的范围为准，达到 V1–V4 且输入匹配后停止。失败、输入变更或特定缺口才补查。 [D09-T01 证据](../2026-11-24-prepare-shadow-plans/tasks.md#final-verification) · [构建说明](../../../../Docs/Build.md) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## A 本地验证

- **CPU 与图权限。** Release 检查通过两种投影的四级端点、一级全覆盖、19–21 m 的过渡带和 0.020694561 m 的吸附步长。四层 4 MiB 预算降到 512 texel，收紧至三层时拒绝整组。图编译接受已完整定义的只读层组，拒绝多层采样视图、层组 owner 写权限和未定义层。 [CPU 日志](../../../../build/d09-t02/a/CsmCpu-Release-handoff.log)
- **原生深度与查询。** Debug 与 Release 在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 的正常帧中，分别核对了 64,464 个过渡样点，其中 1,010 个样点的两级比较值不同。画外 MASK、镜像和双面实例留下可区分深度及诊断颜色，空层为 1，主视图保持 Reverse-Z。深度以图内 CopySource 授权后逐层读入测试 PBO，颜色使用 RHI staging。两个读回入口共用正常帧图及真实完成观察。 [原生 Debug](../../../../build/d09-t02/a/CsmNative-Debug-handoff.log) · [原生 Release](../../../../build/d09-t02/a/CsmNative-Release-handoff.log)
- **帧接受和 Shader 代。** 主 View 两级、离屏 View 四级的后继帧零深度生产 Pass，微移保持同一世界格点。局部运行副本在已录制深度和着色后注入一次图失败，合法前缀取得票据且新颜色与深度均未确认。重试获得本轮结果，移除最后一个 caster 后各层清除。隔离源码改动通过现有热重载链把两路从第 1 代推进到第 2 代，350 个实际 D32 样点按变更上移。关闭或恢复一路诊断时，另一路仍可独立接受。 [原生 Release](../../../../build/d09-t02/a/CsmNative-Release-handoff.log) · [验证入口](../../../../build/d09-t02/a/BuildProbe.ps1)
- **构建和校验。** Windows x64 正式 Debug、Release 构建和两次隐藏窗口启动均退出 0，两配置的六个 StaticForward 角色包通过 ABI 检查。保留早期 MSVC `C1001` 日志，原编译选项下重试成功。OpenSpec 1.13.2 对当前 change 执行严格校验，附本轮源码、包、探针、二进制及日志清单。 [构建 Debug](../../../../build/d09-t02/a/build-debug-handoff-1.log) · [构建 Release](../../../../build/d09-t02/a/build-release-handoff-1.log) · [Shader ABI](../../../../build/d09-t02/a/shader-abi-handoff.log) · [OpenSpec 校验](../../../../build/d09-t02/a/openspec-handoff.json) · [A 交接清单](../../../../build/d09-t02/a/handoff.json)

<a id="b-review-1"></a>
## B 首轮独立审查

- **交接结论**　基准 `ced30710978c20deaa69ab5cac01fdea041e8b2a`。补注前 49 个工作区文件与 A 的 SHA-256 清单逐一匹配。首审退回 A 整改，9 项均开放。5.1 已完成，5.2、5.3 和最终验证仍待各自责任方完成。 [本轮问题单](../../../../build/d09-t02/final/review-signed.md) · [指纹](../../../../build/d09-t02/b-review/annotations.json)
- **两项 P1**　诊断 Shader 的实际材质读取可越过换代检查，并使下一次正常提交失败。主视口在画外 caster 准备失败时丢弃可读旧图的显示机会。 [CR-001](../../../../build/d09-t02/final/review-signed.md#cr-001) · [CR-002](../../../../build/d09-t02/final/review-signed.md#cr-002)
- **七项 P2**　正常 UI 接纳旧阴影请求候选，离屏诊断被基础模式的 FXAA 预算误拒，大世界坐标的投影丢失接收角点；缓存命中仍反复预留上传，画外 caster 编码未计入帧统计，深度程序复制失败后重试丢失摘要，视口标签沿用基础模式。 [CR-003](../../../../build/d09-t02/final/review-signed.md#cr-003) · [CR-004](../../../../build/d09-t02/final/review-signed.md#cr-004) · [CR-005](../../../../build/d09-t02/final/review-signed.md#cr-005) · [CR-006](../../../../build/d09-t02/final/review-signed.md#cr-006) · [CR-007](../../../../build/d09-t02/final/review-signed.md#cr-007) · [CR-008](../../../../build/d09-t02/final/review-signed.md#cr-008) · [CR-009](../../../../build/d09-t02/final/review-signed.md#cr-009)
- **验证和注释**　B 重跑已登记的 Release CPU、Intel OpenGL 回放，原场景的分割、逐层深度、过渡和拒绝后重试通过。另以 RTX 4080 Laptop 的 Release 原生路径检查准备、换代、接受和 UI 选择，以 CPU 相对坐标单独核查投影范围。CR-009 依据视口标签的直接消费代码。36 个产品文件完成审读，32 个有注释编辑，六个 StaticForward 程序以补注后源码打包并通过 ABI 检查。Debug 构建及启动结果沿用 A 的交接记录。 [Intel 基线](../../../../build/d09-t02/b-review/baseline-native.log) · [RTX 反例](../../../../build/d09-t02/b-review/replay.log) · [CPU 数值](../../../../build/d09-t02/b-review/volume.log) · [Shader 打包](../../../../build/d09-t02/b-review/shader-check.log)

<a id="a-correction-1"></a>
## A 首轮整改

- **当前交接。** 以 B 补注后 36 个产品文件 SHA-256 逐一匹配的工作区为输入，按 CR-001 至 CR-009 修正。问题单保留 B 发现、补注记录及各项 A 证据，5.2 已完成，5.3 待 B 使用当前清单复核。 [问题单](../../../../build/d09-t02/final/review-signed.md) · [当前清单与运行入口](../../../../build/d09-t02/a-correction-1/handoff.json)
- **Shader、UI 与附件反例。** Debug、Release 都拒绝诊断片元新增的 `normalImage` 活动读取，合法调色修改在两路由旧有效代切换到新接受代。视口回放覆盖首次失败占位、有旧图的准备拒绝及上传 Pending、同尺寸的陈旧诊断候选、无分配的当前 Forward 候选和恢复。2560 × 2304 离屏对照确认有诊断时采用实际 AA None，真正进入 FXAA 的物理预览执行原预算。 [Release](../../../../build/d09-t02/a-correction-1/Correction-Release.log) · [Debug](../../../../build/d09-t02/a-correction-1/Correction-Debug.log)
- **投影、复用及计数。** Release CPU 核对接收角点的实际编码覆盖、无法保持吸附的远坐标拒绝、既有 0.020694561 m 吸附步长和图层组权限。双 View 三槽暖机后在九个继任帧中均为零深度重绘与零上传需求。移动一个画外 caster 产生一次对象编码、一次 240 B 写入，后继归零。主路失败且离屏路接受时，已发生的编码仍归入本轮。真实主深度和阴影深度的定点复制失败及重试核对了阶段句柄和摘要。 [角点投影](../../../../build/d09-t02/a-correction-1/Volume-Release.log) · [V1 与图层权限](../../../../build/d09-t02/a-correction-1/CsmCpu-Release.log) · [原生计数](../../../../build/d09-t02/a-correction-1/Correction-Release.log)
- **正式与原生检查。** Windows x64 正式 Release、Debug 构建、隐藏启动和六个 StaticForward 角色包的 ABI 校验通过。两配置使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 重跑原生 D32、主 Reverse-Z、MASK 及过渡读回，辨认 1,010 个两级比较不同的样点。合法深度换代后有 350 个 D32 样点按变更移动，两路重新生产完整组并可再次命中。局部探针源码只用于图访问观察、读回、总量准入控制和标签采集。 [Release 构建](../../../../build/d09-t02/a-correction-1/build-release-current.log) · [Debug 构建](../../../../build/d09-t02/a-correction-1/build-debug-current.log) · [Shader ABI](../../../../build/d09-t02/a-correction-1/shader-abi.log) · [Release 深度](../../../../build/d09-t02/a-correction-1/Depth-Release.log) · [Debug 深度](../../../../build/d09-t02/a-correction-1/Depth-Debug.log) · [回放命令](../../../../build/d09-t02/a-correction-1/CheckCurrent.ps1) · [OpenSpec 严格校验](../../../../build/d09-t02/a-correction-1/openspec-strict.json)

<a id="b-recheck-1"></a>
## B 首次整改复核

- **结论**　基准 `ced30710978c20deaa69ab5cac01fdea041e8b2a`。A 交接的 55 个工作区文件与实际差异相符，115 个检查输入、80 个构建产物和 16 份日志的 SHA-256 全部匹配。B 在 18 个整改涉及的产品文件中更新了 7 个文件的注释，复核前后的 42 个产品文件具有相同的非注释内容。本次未签收，5.3 保持待办。 [问题单](../../../../build/d09-t02/final/review-signed.md) · [A 版本核对](../../../../build/d09-t02/b-recheck-1/inputs.json) · [B 补注后指纹](../../../../build/d09-t02/b-recheck-1/review-inputs.json)
- **已关闭 7 项**　CR-001、002、003、006、007、008、009 的原反例均通过。B 重链的 Windows x64 Release / Intel RaptorLake-S Mobile OpenGL 4.6 回放核对了 Shader 拒绝与合法发布、两路候选与 UI 退路、静态零上传、真实编码计数、深度程序复制失败后的重试和实际叠字。另一次回放在 64,464 个过渡点中区分出 1,010 个两级结果不同的点，合法深度修改使 350 个 D32 样点发生预期位移。Debug 结果引用 A 的匹配记录。 [整改场景](../../../../build/d09-t02/b-recheck-1/corrections.log) · [深度读回](../../../../build/d09-t02/b-recheck-1/depth.log)
- **原编号继续整改**　CR-004 的四级离屏诊断已能使用正确的线性附件预算。只有点光而实际回退到超限的物理 FXAA 时，离屏票据为 0，阴影统计仍可能保留 6 个区域。CR-005 正确拒绝了原反例的大坐标无效拟合，但在世界原点拒绝了沿光轴上游 8,192 m、可由 D32 表示的实际 caster。 [CR-004](../../../../build/d09-t02/final/review-signed.md#cr-004) · [CR-005](../../../../build/d09-t02/final/review-signed.md#cr-005) · [离屏对照](../../../../build/d09-t02/b-recheck-1/extras.log) · [正式 View 与数值](../../../../build/d09-t02/b-recheck-1/upstream.log)
- **整改引入的两项回归**　CR-010 [P1] 在深度程序换代后补做 caster 准备，单次分配失败逃出 `RenderFrame`。CR-011 [P2] 在同一 Scene 内容代只读导入深度时，已接受颜色丢失画外 BLEND 投射物的跳过原因。下一轮由 A 按这两项和 CR-004、005 整改。 [CR-010](../../../../build/d09-t02/final/review-signed.md#cr-010) · [CR-011](../../../../build/d09-t02/final/review-signed.md#cr-011) · [原生反例](../../../../build/d09-t02/b-recheck-1/extras.log)

<a id="a-correction-2"></a>
## A 第 2 轮整改

- **交接。** 当前工作树起点与 B 补注后 42 个产品文件的 SHA-256 一致。CR-004、CR-005、CR-010、CR-011 的 A 自检已经完成。既有 7 项复核结论留存，5.3 等待 B 核对新版本和注释。 [本轮问题单](../../../../build/d09-t02/final/review-signed.md) · [整改版清单](../../../../build/d09-t02/a-correction-2/handoff.json)
- **CR-004、005。** 两条点光附件拒绝路径都交付 `SceneAttachmentLimit`、零活动区域和六个释放区域。主 View 的区域代与可用颜色保持稳定，离屏恢复小尺寸后原子准入六面。正式 `ViewWork` 准入 4,096 m 和 8,192 m 的画外 caster。用实际交付的 FP32 投影核对八个 receiver 角点及正向偏移，两组的 XY 尺度与无 caster 对照相同。原远原点拒绝、分割、吸附和层组权限回归通过。 [离屏原生对照](../../../../build/d09-t02/a-correction-2/extras-Release.log) · [上游拟合](../../../../build/d09-t02/a-correction-2/Upstream-Release.log) · [远原点拟合](../../../../build/d09-t02/a-correction-2/Volume-Release.log) · [CPU 基线](../../../../build/d09-t02/a-correction-2/CsmCpu-Release.log)
- **CR-010、011。** 两配置沿正式帧入口在深度程序切换后注入一次 caster 准备分配失败。主路走安全 UI 保留旧颜色，离屏路同帧接受四层。重试仅重绘主路两层并接受新代，静态后继继续导入。画外 BLEND 的跳过摘要随已接受深度跨槽保留，和当前主表面降级同时可见。恢复支持的材质后接受的颜色清除原摘要。 [Release 回放](../../../../build/d09-t02/a-correction-2/extras-Release.log) · [Debug 回放](../../../../build/d09-t02/a-correction-2/extras-Debug.log)
- **正式验证范围。** Windows x64 Debug、Release 正式构建、隐藏启动和六个 StaticForward 角色包的 ABI 检查通过。在 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 上，两配置仍读回 64,464 个过渡样点，其中 1,010 个区分两级比较，合法深度换代移动 350 个实际 D32 样点。Release 另重跑首轮原生反例，包括较大尺寸下的四级 CSM、静态零上传和合法调色换代。长轴拟合的 8,192 m 对照来自 CPU `ViewWork` 与其交付的 FP32 投影。 [Release 构建](../../../../build/d09-t02/a-correction-2/build-release-current.log) · [Debug 构建](../../../../build/d09-t02/a-correction-2/build-debug-current.log) · [Shader ABI](../../../../build/d09-t02/a-correction-2/shader-abi.log) · [Release 深度](../../../../build/d09-t02/a-correction-2/depth-Release.log) · [Debug 深度](../../../../build/d09-t02/a-correction-2/depth-Debug.log) · [首轮反例回放](../../../../build/d09-t02/a-correction-2/Correction-Release.log) · [检查入口](../../../../build/d09-t02/a-correction-2/CheckRound2.ps1) · [OpenSpec 严格校验](../../../../build/d09-t02/a-correction-2/openspec-strict.json)

<a id="b-recheck-2"></a>
## B 第 2 轮整改复核

- **结论**　CR-005、CR-010、CR-011 本次关闭，CR-004 继续使用原编号。累计关闭 10/11，5.3 待签收。A 交接的 55 个工作区文件、完整 Git 差异、81 个产物、18 份日志和 121/123 个检查输入匹配；两份旧工作包的测试头文件已缺失。B 使用匹配的登记二进制重放原检查，独立对照直接编译本轮源码。 [问题单](../../../../build/d09-t02/final/review-signed.md) · [输入及复核版本](../../../../build/d09-t02/b-recheck-2/review-inputs.json)
- **CR-004 的剩余差异**　开启 CSM 时，离屏附件拒绝已经返回正确原因和零活动区域。若上次已交付点光六面，在同一帧改为聚光并超出物理 FXAA 预算，后置拒绝报告只释放 1 个区域；保持相同输入再次拒绝，又报告释放 1 个并推进分配代。关闭 CSM 的同场景依次释放 6、0，第二次分配代稳定。两条路径票据都为 0，主 View 都保有聚光区域。 [CR-004](../../../../build/d09-t02/final/review-signed.md#cr-004) · [正式 Runtime 对照](../../../../build/d09-t02/b-recheck-2/repeat.log)
- **通过项**　Release CPU 验证 4,096/8,192 m 画外 caster 准入及远坐标无效投影拒绝，B 独立编译的斜光轴对照也保持 XY 尺度及接收角点覆盖。Debug、Release 的深度程序换代分配失败只影响主路，离屏同帧获接受，次帧主路恢复。BLEND 投射物摘要在生产和只读导入间一致，恢复 OPAQUE 后清除。真实 OpenGL 读回仍能辨认 1,010 个两级比较不同的样点及 350 个随深度程序移动的样点。 [斜轴投影](../../../../build/d09-t02/b-recheck-2/oblique.log) · [Release 整改回放](../../../../build/d09-t02/b-recheck-2/extras-Release.log) · [Debug 整改回放](../../../../build/d09-t02/b-recheck-2/extras-Debug.log) · [逐层读回](../../../../build/d09-t02/b-recheck-2/depth-Release.log)

<a id="a-correction-3"></a>
## A 第 3 轮整改

- **CR-004。** 以 B 第 2 轮补注后 42 个产品文件逐一匹配的工作树为输入。离屏普通 FXAA 超限且只容纳级联诊断时，先核对诊断消费和方向光准入，再交付区域版本。点光切聚光的同帧超限首次释放上次交付的六个区域，连续拒绝保持零释放及分配代不变。缩回合法尺寸恢复聚光单面和实际 FXAA，主路聚光画面持续接受。2560 × 2304 的四级诊断正向对照仍接受，实际 AA 为 None。 [问题单](../../../../build/d09-t02/final/review-signed.md#cr-004) · [Release](../../../../build/d09-t02/a-correction-3/repeat-Release.log) · [Debug](../../../../build/d09-t02/a-correction-3/repeat-Debug.log)
- **验证与交接。** Windows x64 正式 Debug、Release 构建和隐藏启动通过。定向回放重链当前正式库及 `Runtime/Frame.cpp`，在 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 运行。既有逐层读回及投影回归保留 B 第 2 轮相应版本的证据。OpenSpec 严格校验通过，5.3 保持待 B 签收。 [检查入口](../../../../build/d09-t02/a-correction-3/CheckRepeat.ps1) · [Release 构建](../../../../build/d09-t02/a-correction-3/build-release.log) · [Debug 构建](../../../../build/d09-t02/a-correction-3/build-debug.log) · [OpenSpec](../../../../build/d09-t02/a-correction-3/openspec-strict.json) · [整改版清单](../../../../build/d09-t02/a-correction-3/handoff.json)

<a id="b-recheck-3"></a>
## B 第 3 轮整改复核

- **结论**　CR-004 已关闭，原 11 项全部通过。新增 CR-012 [P2] 为离屏方向光规划的失败原因丢失，当前版本尚未签收。55 个工作区文件及 Git 差异、11 个检查输入、78 个构建产物、11 份日志与 A 第 3 轮清单相符。B 通读 6 个整改文件，在 3 个文件补充注释。复核前后 42 个产品文件的非注释内容一致。 [问题单](../../../../build/d09-t02/final/review-signed.md) · [输入及补注后指纹](../../../../build/d09-t02/b-recheck-3/review-inputs.json)
- **CR-004**　Intel OpenGL 的 Debug、Release 登记程序与 B 独立编译的 NVIDIA Release 回放均确认，开启和关闭 CSM 时，从点光六面切至聚光的首次离屏超限均释放 6 个区域，再次拒绝释放 0 且分配代稳定。主 View 继续出图，离屏缩回 320 × 240 后恢复一层聚光及实际 FXAA；有效四级诊断在 2560 × 2304 使用 AA None。 [Intel Debug](../../../../build/d09-t02/b-recheck-3/repeat-Debug.log) · [Intel Release](../../../../build/d09-t02/b-recheck-3/repeat-Release.log) · [NVIDIA 独立回放](../../../../build/d09-t02/b-recheck-3/replay.log)
- **CR-012**　同一空接收区间在小尺寸产生 `NoReceiverCoverage`，在仅能容纳 CSM 的大尺寸丢失该原因。有效输入的一次 CPU 计划分配失败也仅产生布局不可用与普通附件超限信息，保持输入重试即接受四级诊断。下一次复核需确认布局拒绝、区域结算和实际规划首因可分别追溯。 [CR-012](../../../../build/d09-t02/final/review-signed.md#cr-012) · [复现与断言](../../../../build/d09-t02/b-recheck-3/Reasons.inc)

<a id="a-correction-4"></a>
## A 第 4 轮整改

- **CR-012。** 以 B 第 3 轮补注后 42 个产品文件逐一匹配的工作树为输入。阴影规划在清理失败候选时继续按旧交付区域结算，同时交付当前方向光原因和实际规划拒绝计数。相同空接收区间在大尺寸重新计算及同尺寸命中缓存时均报告 `NoReceiverCoverage`、1 盏拒绝灯。单次 CPU 规划分配注入失败时报告 `StorageUnavailable`、1 盏拒绝灯。三种拒绝都保留 `SceneAttachmentLimit`、零区域和原分配代，主 View 继续接受新图，原参数重试接受四级 CSM。 [问题单](../../../../build/d09-t02/final/review-signed.md#cr-012) · [Release](../../../../build/d09-t02/a-correction-4/replay-Release.log) · [Debug](../../../../build/d09-t02/a-correction-4/replay-Debug.log)
- **验证与交接。** Windows x64 正式构建和隐藏启动及 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 的两配置定向回放通过。回放重链当前库及正式 `Runtime/Frame.cpp`，分配注入只在测试副本的候选分配入口。CR-004 的两路径均保持首次释放 6、重复释放 0 及恢复输出。5.3 等待 B 复核 CR-012。 [检查入口](../../../../build/d09-t02/a-correction-4/CheckReasons.ps1) · [Release 构建](../../../../build/d09-t02/a-correction-4/build-release.log) · [Debug 构建](../../../../build/d09-t02/a-correction-4/build-debug.log) · [OpenSpec](../../../../build/d09-t02/a-correction-4/openspec-strict.json) · [整改版清单](../../../../build/d09-t02/a-correction-4/handoff.json)

<a id="b-recheck-4"></a>
## B 第 4 轮复核与最终签收

- **签收版本**　基准 `ced30710978c20deaa69ab5cac01fdea041e8b2a`，分支 `v2.0`。A 的 55 个工作区文件及 Git 差异、17 个检查输入、78 个产物、11 份日志与清单一致。B 补注后，42 个产品文件的聚合 SHA-256 为 `c122e77324a8704672e286eb20515f59402f1a0a73da7b3067ac84470b2da934`。仅 `ShadowPlan.cpp` 的说明文字与本轮 A 交接不同。CR-001 至 CR-012 全部关闭，5.3 已签收，6.1–6.3 由 A 继续。 [问题单](../../../../build/d09-t02/final/review-signed.md) · [逐文件签收清单](../../../../build/d09-t02/b-recheck-4/review-inputs.json)
- **CR-012**　重新计算和命中缓存均保留方向光空区间的 `NoReceiverCoverage`；单次规划分配失败保留 `StorageUnavailable`。布局仍按普通 FXAA 超限拒绝，主 View 持续接受颜色。B 用补注后的规划源码独立重链混合灯组：空区间只计 1 盏方向光拒绝，旧点光区域先释放 6、后释放 0，分配代稳定；全局存储失败则计 2 盏，原参数重试接受四级方向光与点光六面。 [复核条目](../../../../build/d09-t02/final/review-signed.md#cr-012) · [独立回放](../../../../build/d09-t02/b-recheck-4/mixed.log)
- **验证范围**　Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。B 实跑登记的 Debug、Release 两配置检查及独立 Release 对照，均通过。完整产品构建与隐藏启动引用本轮 A 证据；此前 D32 数值、遮挡、过渡及换代读回按前轮版本留存。 [Debug](../../../../build/d09-t02/b-recheck-4/replay-Debug.log) · [Release](../../../../build/d09-t02/b-recheck-4/replay-Release.log) · [A 第 4 轮交接](#a-correction-4) · [前轮 D32 证据](../../../../build/d09-t02/b-recheck-2/depth-Release.log)

<a id="final-verification"></a>
## A 最终验证

逐文件核对 B 签收的 42 个产品文件，源码聚合 SHA-256 为 `c122e77324a8704672e286eb20515f59402f1a0a73da7b3067ac84470b2da934`。CR-001 至 CR-012 及任务 5.3 均已签收。原问题单逐字节留存，快照内的相对引用以原文件 `Docs/CodeReview.md` 为起点。 [签收输入](../../../../build/d09-t02/final/signed-inputs.json) · [B 签收原件](../../../../build/d09-t02/final/review-signed.md) · [B 复核结论](#b-recheck-4)

| 规范 | 实现与验证 |
|---|---|
| `shadow-planning` 的有限投影、区域分配、内容代 | `ShadowVolumes` 负责固定 XY 扫描和 Z 拟合，`ShadowPlan` 负责预算内成组分配及逐级失效。最终 CPU 回放验证透视与正交分段、19–21 m 过渡、画外 caster、跨格和 4 MiB 预算降档。混合灯组核对区域释放和重试，远坐标、长轴及斜光轴沿用 B 在拟合行为相同版本上的证据。 [投影](../../../../Engine/Renderer/Lighting/ShadowVolumes.cpp) · [分配](../../../../Engine/Renderer/Lighting/ShadowPlan.cpp) · [CPU 回放](../../../../build/d09-t02/final/Cpu-Release.log) · [长轴](../../../../build/d09-t02/b-recheck-2/Upstream.log) · [斜光轴](../../../../build/d09-t02/b-recheck-2/oblique.log) |
| `shadow-rendering` 的深度生产、级联查询、接受复用 | 逐层 D32 写入与样本查询沿用同一套投影和共享覆盖。两个配置均读回四层，核对 MASK 孔、负缩放、双面、空层及主 Reverse-Z。64,464 个过渡样点通过，其中 1,010 个区分相邻层比较。深度 Shader 换代使 350 个 D32 样点发生预期位移，图前缀未推进接受身份，重试及两路独立复用通过。 [图生产](../../../../Engine/Renderer/Paths/Forward.cpp) · [级联查询](../../../../Shaders/Shadow/Directional.glsl) · [Release 深度](../../../../build/d09-t02/final/Depth-Release.log) · [Debug 深度](../../../../build/d09-t02/final/Depth-Debug.log) |
| `render-graph-execution` 的资源交接 | 编译器只允许由逐层能力支持且全部有定义的范围作为不可变只读层组出口。CPU 对照拒绝多层单视图、可写出口及未定义层。原生回放核对真实导出、后继导入和失败前缀后的重试。 [图准入](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp) · [CPU 权限](../../../../build/d09-t02/final/Cpu-Release.log) · [帧深度](../../../../build/d09-t02/final/Depth-Release.log) |
| `static-forward-rendering` 的每 View 级联诊断 | 正常 Forward 图按请求选择线性诊断，运行中读取当前级联与过渡数据。回放核对两路输出、2560 × 2304 诊断的实际 AA None 及普通 FXAA 超限的退路。面板标签、旧图回退和同尺寸候选身份沿用对应 B 复核。 [运行时入口](../../../../Runtime/Frame.cpp) · [混合灯组](../../../../build/d09-t02/final/Mixed-Release.log) · [陈旧候选与旧图](../../../../build/d09-t02/b-recheck-1/corrections.log) |
| `view-preparation` 的请求、独立计划与拒绝原因 | CPU 回放核对越界级数的拒绝与原请求保留，原生回放核对无区域的普通退路、连续拒绝释放 6 和 0、保持分配代、布局恢复以及 `NoReceiverCoverage` 与 `StorageUnavailable` 两层诊断。受控存储失败仅作用于测试副本的单次规划。 [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [签收版原生回放](../../../../build/d09-t02/final/Mixed-Release.log) |

核对了 9 条新增或修改的要求和 25 个场景的实现与证据。Windows x64 正式 Debug、Release 重建、隐藏启动及两配置各六个 StaticForward 包的 ABI 校验均通过。最终原生回放使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。CPU 和 D32 回放复用已登记的断言，使用独立测试入口替代已清理的旧头文件，图中探针的完成观察走 RHI EventQuery。产品文件保持 B 签收版本。 [运行入口](../../../../build/d09-t02/final/CheckFinal.ps1) · [源码和构建证据](../../../../build/d09-t02/final/native-verify.json) · [同步前严格校验](../../../../build/d09-t02/final/pre-sync-validation.json)

<a id="spec-sync"></a>
## 规范同步

四份现有主规范同步完成。`render-graph-execution` 更新 1 条资源交接要求，`shadow-planning` 更新 3 条要求，`view-preparation` 更新 1 条要求，`static-forward-rendering` 新增 1 条诊断要求。新增 `shadow-rendering` 主规范保留增量中的 Purpose 及 3 条要求。合并保留继承场景，其余 37 份主规范的 SHA-256 与同步前一致。逐份复查五份增量均已同步。OpenSpec 1.13.2 严格校验通过 42 份主规范及当前变更。 [同步核对](../../../../build/d09-t02/final/spec-sync.json) · [主规范校验](../../../../build/d09-t02/final/main-validation.json) · [变更校验](../../../../build/d09-t02/final/synced-validation.json)

<a id="archive-closeout"></a>
## 归档

D09-T02 已按 OpenSpec 1.13.2 归档，全部 15 项任务完成。开发计划保留验收链接，D09-T03 前置已满足，等待立项。保留 B 签收原件后，当前问题单按模板复位。 [归档回执](../../../../build/d09-t02/final/archive.json) · [归档核对](../../../../build/d09-t02/final/archive-audit.json) · [D09-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02) · [D09-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03)
