# Tasks

## 1. View 请求与有限过滤覆盖

- [x] 1.1 扩展 View 的会话阴影请求与准备，校验过滤、逻辑额度及有效边长，沿用实际显示分支、规划准入和离屏结算。通过类型化入口以共享 Scene 的两路 View 核对合法档切换、非法值整体拒绝、另一 View 的既有请求及计划身份保持。预算场景用 512 texel 下 8/16 MiB 两档区分完整点光组拒绝和准入，核对原灯集合。复用未变的默认请求与场景 v1 用例。 [View](specs/view-preparation/spec.md)
- [x] 1.2 让 `ShadowPlan` 交付最大核的投影、分辨率和 caster 余量。固定布局以 CPU 数值证据覆盖级联吸附/过渡、窄聚光的打包精度与点光交角的最外比较位置。核对已知画外遮挡在扩展候选中、偏移留在同一投影及 Hard/PCF 切换不推进内容代。复用 D09-T02/T03 未改的有限范围和不合法投影证据，按新投影影响选代表性边界。 [规划](specs/shadow-planning/spec.md)

## 2. D32 数组的接口、绑定与图权限

- [x] 2.1 接通生成接口、包及 OpenGL 的普通浮点数组 SRV。用受影响的正式包验证 layout、活动资源维度和逻辑身份，原生读取具有可区分深度的非零起始层视图，覆盖单层数组保持数组维度。用一个失配接口和实际错误绑定/超界范围证明相应边界拒绝，不再重复证明旧二维或 cube 的全部情况。 [Shader](specs/shader-build/spec.md) · [OpenGL](specs/opengl-shader-pipelines/spec.md)
- [x] 2.2 令图按设备核验过的同格式能力声明和落实精确的数组读及逐层写，在实际权限下解析 SRV。复用上项 D32 载荷，核对每个层的定义版本、写后读状态、图尾范围及接受后只读导入。以未定义层和扩大实际绑定范围作为负例，验证相应边界拒绝，并保留既有逐层保留入口。 [图执行](specs/render-graph-execution/spec.md)

## 3. 深度生产及共享逐灯查询

- [x] 3.1 把各 View 实际消费的准入组映射到紧凑数组并接入 Forward。共用静态 caster Draw、MASK、镜像、双面与空层清除，保持当前预算及有限消费。以同时包含 CSM、spot、point 和无阴影分配灯的图核对同灯对应关系及活层，包含逻辑层离散的点光和一个空面。核对非受光分支与单组诊断的实际 GPU 工作范围。 [阴影生产](specs/shadow-rendering/spec.md)
- [x] 3.2 实现共用 Hard、PCF、几何接收偏移、级联混合和点光扩面查询。用深度 0.25/0.75 的已知九点核将 Hard=0、PCF=5/9 与错误的 raw-depth 平均区分，验证 Hard 等深可见。通过实际遮挡及无自遮挡的代表表面核对偏移方向和尺度，在最大核下核对级联两级、spot 外锥和 point 面边/角使用当前有效内容。 [可见性](specs/shadow-rendering/spec.md#requirement-resolve-hard-and-pcf-from-conventional-depth-comparisons)
- [x] 3.3 发布同轮灯行与组引用，经共享 `EvaluateLighting` / `EvaluateDirect` 接入受光变体，现有诊断使用同一查询。用可分色的灯验证换序、灯槽复用与当前区域代匹配，shadowDistance 或预算拒绝的灯仍按自身物理参数出光。核对受影响的 Direct、Physical、IBL 及诊断正式包和变体从现有准备链加载，无阴影模式照常可用。 [灯光](specs/punctual-lighting/spec.md) · [Forward](specs/static-forward-rendering/spec.md)
- [x] 3.4 沿图及设备真实完成合同交付、复用和退休 View 数组。原生核对冻结输入的零深度重绘、仅过滤变化时的复用，以及一次深度内容编辑后前缀或未接受候选的重试。记录匹配正常提交、旧输出身份、实际在途保活及真正完成，另一 View 继续独立接受。 [复用与接受](design.md#depth-reuse-and-statistics)

## 4. 质量面板与有界统计

- [x] 4.1 将阴影、过滤、边长及逻辑预算接入 Renderer 面板，并从现有 Runtime 快照发布规划和已接受事实。核对请求与生效值、拒绝原因、复用与生成层数和绘制批次、计划字节与实际数组逻辑字节。比较同一提交序列的面板显隐，旧接受票据和 GPU 计时的 Pending/Ready 保持可区分。 [统计](specs/render-frame-statistics/spec.md)

## 5. Agent-A 集成验证及交接

- [x] 5.1 运行受影响的 Windows x64 目标及真实 OpenGL Forward，核对生成包和实际执行身份。以固定曝光及未饱和的预显示 HDR 样点比较阴影关闭、Hard、PCF。用无阴影同输入的逐灯直接贡献作为参照，验证 `间接光与自发光 + Σ(对应可见性 × 各灯直接贡献)`，并覆盖无阴影预算灯、遮挡外样点、Unlit 及原诊断。用同一场景在 Direct 和带环境/发光的 Physical 分支复核，必要时复用前述原生检查样点。 [HDR](specs/static-forward-rendering/spec.md) · [构建与运行](../../../../Docs/Build.md)
- [x] 5.2 按 [测量合同](../../../../Docs/Architecture.md#benchmark) 在同一产品构建和 OpenGL 设备上采集 Off、Hard、PCF。固定场景内容、相机、分辨率、曝光、其他画质、窗口/VSync、预热和采样数，区分首次生产与静态复用。记录 CPU 各阶段含图声明/执行、GPU 已就绪区间的 median/p95、等待与 Draw/上传/绑定/层字节等计数及各自范围。用匹配提交的计时，记录无结果的样本与设备限制。该比较交付本工作包的增量成本，不设置 D09-T05 压力门槛。
- [x] 5.3 汇总实际命令、运行/复用结果、限制与匹配的源、场景、包和可执行文件身份。验证通过后用 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md) 交接 A 开发与本地检查结果，留待独立审查。开发专用输入和输出保存在 `build/d09-t04/`，沿最小风险验证原则，仅在失败、输入变化或发现具体覆盖缺口时扩大。

## 6. 独立审查与收尾

- [x] 6.1 Agent B 独立核对实现、实际消费者和可匹配验收证据，完成涉及的第一方注释及审查问题登记。 [首轮结论](#b-review)
- [x] 6.2 Agent A 按 CR 编号整改和自检，记录可复核的结果及修正版本。无整改项时按问题单实际状态注明。 [首轮整改](#a-correction-1) · [CR-005 再整改](#a-correction-2)
- [x] 6.3 Agent B 在匹配版本上独立复核，检查实际修正、受影响消费者及全部涉及注释，给出最终验收结论。 [首轮整改复核](#b-recheck-1) · [B 最终签收](#b-recheck-2)
- [x] 6.4 Agent A 在 B 验收的版本上执行最终 verify，确认源与实际原生证据的配对，留存 CR、验收结果及限制。 [最终验证](#final-verification)
- [x] 6.5 同步能力规格，验证主规格和链接后归档 change，更新 D09-T04 状态及下一个工作包入口。保留签收证据并在成功归档后按模板重置当前问题单。 [归档验收](#archive-closeout)

<a id="a-local-verification"></a>
## A 本地验证

本轮使用 Windows x64/MSVC 和 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0 Build 32.0.101.6129。正式 Release、Debug 构建和隐藏启动已通过。证据通过当前产品库和生成包重链。开发用图观察器与统计面板显隐入口只存在于 `build/d09-t04/a/`。 [复现入口](../../../../build/d09-t04/a/Verify.ps1) · [Release 构建](../../../../build/d09-t04/a/product-Release-final.log) · [Debug 构建](../../../../build/d09-t04/a/product-Debug.log)

| 合同 | 实际结果 |
| --- | --- |
| View 与投影 | 在 512 texel 下，8 MiB 拒绝完整点光组，16 MiB 接受四级方向光、一个 spot 视图和六个 point 面。非法过滤/预算整体拒绝。切换 PCF 时计划和内容代复用，附加 View 保持独立。旋转 `1e-8 rad` 拒绝，`0.00030 rad` 恢复。级联吸附与过渡沿前置工作包的固定场景重跑。 [CPU](../../../../build/d09-t04/a/Cpu-Release.log) · [级联及图 CPU](../../../../build/d09-t04/a/CsmCpu-Release.log) |
| Shader、图与原生数组 | `texture2DArray` 维度贯通生成、活动反射与原生 SRV。二维 IR 替代数组声明时拒绝。非零起始物理层以单层数组视图读取，Hard 为 0，PCF 为 RGBA16F 的 0.555176，等深均为 1。原生拒绝越界视图，图拒绝未定义层以及将单层授权扩大为两层的实际绑定。后继只读导入只使用匹配接受内容。 [包](../../../../build/d09-t04/a/array-package.log) · [原生](../../../../build/d09-t04/a/array-Release.log) · [前缀与重试](../../../../build/d09-t04/a/checks-Release.log) |
| 深度、可见性与保活 | 实际图从 11 层扩为 16 层，逻辑层可离散，物理层按各灯完整组紧排。Spot 诊断只读一层，point 只读六层。点光面边和三面角保持遮挡。方向光空层清除为 1，实际重核对 64,124 个过渡样点，其中 810 个区分了两级的可见性。镜像、MASK 和双面画外 caster 有原生读回。冻结两路及纯过滤切换复用。失败前缀保留旧接受颜色，恢复后重新生产，换代灯只关联当前 generation。读回等待了实际原生完成。 [局部灯与接受](../../../../build/d09-t04/a/checks-Release.log) · [方向光](../../../../build/d09-t04/a/csm-Release.log) |
| 正式 HDR 与界面 | Direct 和 Physical 各核对 76,674 个接收面样点。逐灯 HDR 差由当前可见性解释，覆盖环境、自发光、Unlit 和仍有贡献的无分配点光。后者有 56,606 个蓝通道贡献样点。原生回放使用真实统计面板构建，对比显隐时的 View 画布、接受票据及深度工作；类型化请求和 GPU 计时按各自身份核对。 [HDR、请求与面板](../../../../build/d09-t04/a/checks-Release.log) |

联调时发现普通 Physical 阴影帧仍将深度输入当作诊断标志，导致显示阶段误选线性 clamp，与已准备的 ACES 绑定不符。显示选择已改为检查实际诊断角色，并完成包含 IBL 的 Release 回放。 [显示准备](../../../../Engine/Renderer/Paths/Forward.cpp) · [修正后回放](../../../../build/d09-t04/a/checks-Release.log)

Debug 回放通过原生数组及 Direct HDR，普通 Physical 也已取得接受帧。加上环境后，150 秒的回放窗口内未取得匹配的 Debug IBL 接受帧。包含 IBL 的完整逐灯验收采用已通过的 Release 回放。 [Debug 数组](../../../../build/d09-t04/a/array-Debug.log) · [Debug 观察](../../../../build/d09-t04/a/checks-Debug.log)

<a id="a-performance"></a>
## A 同口径测量

固定一个 640 × 480 的 Direct Lighting View、256 texel、一层 CSM、同一 `LocalScene.inc`、正交相机、f/2、0.5 s 快门及 VSync Off。先记录各档首个接受帧，再预热 6 帧并采集各 72 个接受帧。CPU 工作包含 UI、准备、图声明、编译及执行，槽等待、交换等待另计。GPU 仅汇总在非阻塞快照中观察到的匹配 Ready 票据。 [固定输入和逐阶段统计](../../../../build/d09-t04/a/timing-summary.json) · [原样本](../../../../build/d09-t04/a/timing.csv)

| 档位 | CPU p50 / p95（ms） | GPU p50 / p95（ms） | GPU Ready / 接受帧 | 暖帧原生 Draw / 绑定 | 阴影数组逻辑 B |
| --- | ---: | ---: | ---: | ---: | ---: |
| Off | 0.494 / 1.164 | 1.990 / 3.361 | 24 / 72 | 10 / 36 | 0 |
| Hard | 0.590 / 1.283 | 3.140 / 4.371 | 26 / 72 | 10 / 41 | 2,883,584 |
| PCF | 0.607 / 1.257 | 3.268 / 3.906 | 25 / 72 | 10 / 41 | 2,883,584 |

Hard 首个接受帧生产 11 层，整图 35 次原生 Draw。切到 PCF 的首个接受帧沿用原数组，同静态暖帧均无阴影重绘。三档暖帧原生上传均为每帧 3 笔、38,716 B。计时和计数反映本机的完整正常场景图，包含同图 UI。 [首帧及测量日志](../../../../build/d09-t04/a/bench-Release.log)

<a id="a-handoff"></a>
## A 交接

Agent A 已完成 1.1–5.3。基准为 `eeb5654480fbdd722163ea06d85c1dcfecec340b`，交接清单登记了 48 个变更的产品文件，其中 6 个为新文件。逐文件 SHA-256 汇总为 `5a308d75f2084457119de153a1f61bc7fc652c2cf73b0648733276bc8759b890`。清单同时登记正式包、原生库、可执行文件、场景输入和实际日志。审查以本工作树及 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md) 为准，由维护者调度 B。 [交接清单](../../../../build/d09-t04/a/handoff.json) · [正式已跟踪 diff](../../../../build/d09-t04/a/tracked.patch)

<a id="b-review"></a>
## B 首轮审查与注释

Agent B 核对了 A 的 48 个产品文件及交接身份，补充了 23 个文件的注释。注释后的产品指纹为 `ca4e712fde67442910b71f548165ca17668a24ee6c50ded992f58897cd49de42`。本轮登记 5 项 P2，均待 A 整改。 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md)

| 问题 | B 首轮证据 |
|---|---|
| CR-001 无颜色消费时仍生产阴影 | 正式 Forward 子图中颜色 Pass 已裁剪，内部导出仍保留 11 个深度 Pass 和 11 MiB 数组。 [编译计划观察](../../../../build/d09-t04/b-review/ViewStats.log) |
| CR-002 双 View 的接受统计 | 主视口从票据 5 推进到 6，另一 View 的颜色仍属 5，其阴影统计项消失。 [双路原生观察](../../../../build/d09-t04/b-review/ViewStats.log) |
| CR-003 数组 SRV 同格式边界 | 非零起始层的 RGBA8 数组同时接受同格式和 sRGB 重解释。 [原生绑定结果](../../../../build/d09-t04/b-review/ArrayView.log) |
| CR-004 采样数组的阶段边界 | 正式工具为 Compute 生成采样数组声明，使用同一接口打包时因 profile 拒绝。 [最小声明](../../../../build/d09-t04/b-review/ArrayProfile.json) · [IR 诊断](../../../../build/d09-t04/b-review/ComputeArray.log) |
| CR-005 接收偏移缺少独立验证 | 仅在隔离副本中将接收偏移置零，六个受影响角色重新打包后，HDR 与 CSM 核对仍通过。 [HDR 对照](../../../../build/d09-t04/b-review/Offset-checks.log) · [CSM 对照](../../../../build/d09-t04/b-review/Offset-csm.log) |

B 重跑了 A 交接版本的 Release CPU、CSM CPU、D32 数组、Direct/Physical HDR、双 View 及 CSM 检查，另重跑了 Debug D32 数组。注释后的九个颜色和诊断角色通过 Release 正式工具编译。三档性能引用 A 的同口径测量。含 IBL 的完整 HDR 证据限于 Release；A 的 Debug 回放在 150 秒内未取得匹配接受帧。代码验收待 6.3，注释随整改复核。 [Release 原生 HDR](../../../../build/d09-t04/b-review/Release-checks.log) · [Debug 数组](../../../../build/d09-t04/b-review/Debug-array.log) · [A 的配置限制](#a-local-verification)

<a id="a-correction-1"></a>
## A 首轮整改与自检

A 核对 B 注释后的 48 项产品源文件汇总 `ca4e712fde67442910b71f548165ca17668a24ee6c50ded992f58897cd49de42`。整改基准仍为 `eeb5654480fbdd722163ea06d85c1dcfecec340b`，49 个产品变更含 6 个未跟踪文件，汇总 `dbe1169f93748627b54c7dc1a3f853544527aa3559f2267b7283992bb4e5cc78`。回放从整改后的产品库、生成接口和 Shader 包重链。B 对本次整改的复核见下方记录。 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md) · [B 输入收据](../../../../build/d09-t04/a-correction-1/review-input.json) · [匹配身份](../../../../build/d09-t04/a-correction-1/handoff.json)

| 问题 | 修正和实际结果 |
|---|---|
| CR-001 图裁剪与保留 | Forward 将阴影深度作为普通图版本交付。Runtime 在具有实际颜色消费的路上导出新深度。同一冷候选无消费时为 0 个阴影 Pass、0 B。显式显示消费者得到 11 层，独立深度读回在颜色裁剪时仍提交并映射 D32。正式颜色接受后初次生产 11 层，下一帧复用 11 层。 [编译与原生回放](../../../../build/d09-t04/a-correction-1/view-stats-Release.log) |
| CR-002 双路统计 | 整图正常场景快照沿用单票据，各 View 从自己的最近接受场景派生阴影统计。离屏请求 `InvalidExtent` 期间，主路和整图票据为 6，离屏票据为 5，当前拒绝原因可读。离屏于 8 恢复，10 接受 Off 并交付空阴影工作。 [双路原生回放](../../../../build/d09-t04/a-correction-1/view-stats-Release.log) |
| CR-003 数组 SRV | RGBA8 三层存储的同格式单层数组视图成功，sRGB 数组请求被拒绝并带 slot、layer 诊断，原二维视图继续接受。D32 的非零起始层读取及 Hard、PCF、等深和越界对照通过。 [原生绑定及读取](../../../../build/d09-t04/a-correction-1/array-Release.log) |
| CR-004 Compute 接口 | `generate` 拒绝 B 的原输入，诊断含 `arrayImage`、`texture2d-array` 与 compute，未发布接口。合法图形数组完成打包及原生 Draw，Compute storage 数组通过生成和活动 `imageStore` 打包。 [拒绝](../../../../build/d09-t04/a-correction-1/compute-rejected.log) · [Compute 对照](../../../../build/d09-t04/a-correction-1/compute-storage.log) |
| CR-005 几何接收偏移 | 固定样点位于 D32 采样列分界旁。对正交 CSM 和透视 spot，以两个投影对应的世界 texel 尺度分别检查跨列和深度阈值。Hard 为 `[1,0,1,1]`，PCF 为 `[0.666504,0,0.666504,1]`，与独立预期的 `2/3` 过滤结果相符。仅隔离包置零时四组读回全零，触发预期的像素断言；正式 Shader 内容沿用 B 审查版本。 [固定输入](../../../../build/d09-t04/a-correction-1/Offset.inc) · [查询](../../../../build/d09-t04/a-correction-1/offset-Release.log) · [置零负对照](../../../../build/d09-t04/a-correction-1/offset-zero-Release.log) |

Release 正式构建和隐藏启动通过。首次 MSVC `C1001` 随日志保留，同配置重试成功。Release 同机重跑已有逐灯 HDR、Physical IBL、前缀重试、面板显隐及 CSM 验收。原生设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0 Build 32.0.101.6129。三档计时沿用 A 原交接的同口径记录。 [首轮构建日志](../../../../build/d09-t04/a-correction-1/product-Release.log) · [成功构建](../../../../build/d09-t04/a-correction-1/product-Release-retry.log) · [回放命令](../../../../build/d09-t04/a-correction-1/Verify.ps1) · [HDR 与 IBL](../../../../build/d09-t04/a-correction-1/checks-Release.log) · [CSM](../../../../build/d09-t04/a-correction-1/csm-Release.log)

补充 Debug 构建在 `PhysaraShader` 遇到 MSVC 内部错误。同配置重试和单进程调度仍出现 `C1001`，逐编译单元调度以 `CL.exe` 访问冲突退出。本轮五项原生整改证据均来自当前 Release 产品输入。 [Debug 重试](../../../../build/d09-t04/a-correction-1/product-Debug-retry.log) · [单进程](../../../../build/d09-t04/a-correction-1/product-Debug-serial.log) · [逐编译单元调度](../../../../build/d09-t04/a-correction-1/product-Debug-mtt.log)

<a id="b-recheck-1"></a>
## B 首轮整改复核

复核 A 整改产品指纹 `dbe1169f93748627b54c7dc1a3f853544527aa3559f2267b7283992bb4e5cc78`。CR-001 至 CR-004 已关闭。CR-005 的 spot 验收样点仍待 A 整改，6.3 保持开放。B 核对了全部 8 个整改文件和直接消费者，补充了两个接口的注释。注释后 50 项产品文件汇总为 `e0bd682329aa9924012d8b197868c0703df11801d368af4c0b7d6f325ae5e773`。 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md) · [复核输入与证据身份](../../../../build/d09-t04/b-recheck-1/review-inputs.json)

| 编号 | B 实际检查与结果 |
|---|---|
| CR-001、CR-002 通过 | 无消费者时阴影 Pass 和瞬态字节均为零；独立深度读回有 11 个 Pass 并成功映射。主路票据 6 与离屏旧票据 5 同时可读；离屏以 8 恢复、9 复用、10 接受零消费。 [原生回放](../../../../build/d09-t04/b-recheck-1/view-stats.log) |
| CR-003、CR-004 通过 | 数组异格式在 slot 0、层 1 被拒绝，同格式数组和二维重解释成功。Compute 采样数组在生成阶段被拒且无发布文件；合法 storage 数组打包成功。 [绑定](../../../../build/d09-t04/b-recheck-1/array.log) · [生成拒绝](../../../../build/d09-t04/b-recheck-1/compute-rejected.log) · [storage 对照](../../../../build/d09-t04/b-recheck-1/compute-storage.log) |
| CR-005 部分通过 | CSM 正例及置零负例成立。spot 固定样点手填的细、粗尺度系数为 `1/256`、`1/64`。保持其他输入固定，从正式规划器取得 `1/513`、`1/321` 后，Hard 为 `[0,0,1,0]`，PCF 为 `[0,0,0.666504,0]`，两者均与 A 的预期不符。待用真实系数重新标定独立期望并重跑正负例。 [交接用例](../../../../build/d09-t04/b-recheck-1/baseline-offset.log) · [规划器系数](../../../../build/d09-t04/b-recheck-1/plan-offset.log) · [置零负例](../../../../build/d09-t04/b-recheck-1/zero-offset.log) |

B 使用匹配的 Release 产品库独立重链，并重建两份偏移查询包，包摘要均与 A 交接相同。已有 Direct 和 Physical/IBL 各 76,674 个样点通过，前缀重试、面缝及级联过渡回放通过。三档性能仍引用原始测量。 [HDR](../../../../build/d09-t04/b-recheck-1/checks.log) · [CSM](../../../../build/d09-t04/b-recheck-1/csm.log)

B 在原 Debug 配置重试 `PhysaraShader` 和完整 `Physara` 目标，两者均成功。隐藏启动 `--poll-limit=15` 退出码为 0；使用本次 Debug 库和生成的数组包重链后，CR-001 至 CR-003 的原生回放通过。此前的编译器内部错误保留于 A 的交接记录，原因尚未确定。完整逐灯 Physical/IBL 像素验收仍限于 Release。 [本轮 Debug 构建](../../../../build/d09-t04/b-recheck-1/debug-product.log) · [启动](../../../../build/d09-t04/b-recheck-1/debug/startup.log) · [双 View](../../../../build/d09-t04/b-recheck-1/debug/view-stats.log) · [数组](../../../../build/d09-t04/b-recheck-1/debug/array.log) · [两配置输入与日志身份](../../../../build/d09-t04/b-recheck-1/review-inputs.json)

<a id="a-correction-2"></a>
## A 再整改与自检

CR-005 的首轮 spot 用例以 `H/E` 手填系数，使预期依赖超过规划器交付值的偏移。再整改用 `MakeLocalShadowVolume` 和 `MakeLocalShadowPlan` 取得 256 texel 局部灯的近远面、无偏投影和接收偏移系数，再按实际 `ForwardShadowLayer` ABI 交给正式 `QueryShadow`。两档在同一接收点的世界 texel 分别为 `0.00048828125 m`、`0.001953125 m`，系数为 `1/513`、`1/321`。 [规划输入与 GPU 查询](../../../../build/d09-t04/a-correction-2/OffsetProbe.cpp)

独立几何计算给出原接收深度 `0.860215054`，使用正确朝光侧位移后，深度分别为 `0.860149201`、`0.860109784`。固定边界深度 `0.860182` 核对跨列方向，固定均匀深度 `0.860130` 位于两档之间。Release 原生读取在细、粗尺度边界及均匀区的 Hard 结果为 `[1,0,1,1]`，PCF 为 `[0.666504,0,0.666504,1]`，与 `2/3` 经 RGBA16F 量化一致。CSM 原对照复跑通过。同一探针、D32 数组和规划输入加载仅置零 `ShadowReceiverOffset` 返回值的隔离包后，CSM 和 spot 的 Hard、PCF 都读得全零，随后按预期由像素断言以退出码 1 结束。 [独立参考](../../../../build/d09-t04/a-correction-2/oracle.log) · [正例日志](../../../../build/d09-t04/a-correction-2/offset-Release.log) · [置零日志](../../../../build/d09-t04/a-correction-2/offset-zero-Release.log)

本轮正式范围逐项匹配 B 注释后的 50 项产品指纹 `e0bd682329aa9924012d8b197868c0703df11801d368af4c0b7d6f325ae5e773`。Release 探针使用已验证身份的产品库和生成接口重链，两个查询测试包由正式工具编译。回放设备为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。CR-001 至 CR-004、产品 Debug 构建与启动以及现有 HDR/IBL 验收沿用 B 首轮整改复核记录。本轮交付待 B 复核 CR-005。 [运行入口](../../../../build/d09-t04/a-correction-2/Verify.ps1) · [交接清单](../../../../build/d09-t04/a-correction-2/handoff.json) · [B 已完成的检查](#b-recheck-1)

<a id="b-recheck-2"></a>
## B 再整改复核与最终签收

Agent B 关闭 CR-005，确认五项 CR 全部通过，签收任务 6.3。基准为 `eeb5654480fbdd722163ea06d85c1dcfecec340b`，50 项产品文件（含 6 个未跟踪文件）SHA-256 为 `e0bd682329aa9924012d8b197868c0703df11801d368af4c0b7d6f325ae5e773`。该版本与上轮 B 注释版逐文件相同；代码与注释均通过验收。 [B 签收问题单](../../../../build/d09-t04/final/review-signed.md) · [B 签收清单](../../../../build/d09-t04/b-recheck-2/review-inputs.json)

B 核对了 45 项 Release 构建输入，并在独立目录重编两份查询包、重链回放程序。两份包分别与 A 交接包逐字节匹配。spot 样点消费规划器交付的近面 9/1024 m、远面 9 m、两份投影及偏移系数 1/513 和 1/321。按几何独立计算，两档均从列 128 跨到列 127，偏移后的比较深度分别为 0.860149201 和 0.860109784。均匀区深度 0.860130 将两档区分开。 [独立计算](../../../../build/d09-t04/b-recheck-2/geometry.log)

在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 上，CSM 和 spot 的 Hard 都读回 `[1,0,1,1]`，PCF 都读回 `[0.666504,0,0.666504,1]`，正例退出码 0。保持相同规划、阈值和 D32 数据，仅将接收偏移置零，四组读回全零，像素断言按预期以退出码 1 结束。 [正例](../../../../build/d09-t04/b-recheck-2/offset.log) · [负例](../../../../build/d09-t04/b-recheck-2/offset-zero.log)

本轮复测范围为 Release 的 CR-005。CR-001 至 CR-004、Debug 构建与隐藏启动及其原生数组和双 View 检查沿用首轮整改复核；完整逐灯 Physical/IBL 像素证据来自 Release，三档性能沿用原始测量。6.4 最终 verify 与 6.5 规格同步、归档由 Agent-A 接续。 [首轮整改复核](#b-recheck-1) · [三档测量](#a-performance)

<a id="final-verification"></a>
## A 最终验证

A 对签收基准 `eeb5654480fbdd722163ea06d85c1dcfecec340b` 核对了 50 项产品输入，含 6 个未跟踪 Shader 文件，汇总 `e0bd682329aa9924012d8b197868c0703df11801d368af4c0b7d6f325ae5e773`。构建前逐项匹配 B 的 Release、Debug 产物和回放清单，重建后再次核对源码指纹。CR-001 至 CR-005 的签收记录按原字节留存，原件的相对链接以当时的 `Docs/CodeReview.md` 为起点。 [签收输入](../../../../build/d09-t04/final/signed-inputs.json) · [签收原件](../../../../build/d09-t04/final/review-signed.md) · [B 验收结论](#b-recheck-2)

| 复核范围 | 对应实施与签收版结果 |
| --- | --- |
| `view-preparation`、`shadow-planning` | View 的请求校验与同轮准备使用自己的相机和阴影输入。CPU 回放核对不同预算、非法过滤和档位、过滤切换复用、有限局部投影、窄锥拒绝与恢复、稳定 CSM 选级和吸附。 [请求入口](../../../../Engine/Renderer/View.cpp#L190) · [投影规划](../../../../Engine/Renderer/Lighting/ShadowVolumes.cpp#L564) · [局部灯和请求](../../../../build/d09-t04/final/Cpu-Release.log) · [级联与图](../../../../build/d09-t04/final/CsmCpu-Release.log) |
| `shadow-rendering` | Forward 逐层生产或只读导入完整数组，原生回放确认 CSM 过渡、MASK 孔、画外 caster、镜像、双面、空层及点光面缝。Hard 读到遮挡样点为 0，PCF 为 0.555176，Hard 等深为 1。正式规划输入的双尺度偏移在 CSM 和 spot 均通过几何预期，置零导致四组读回全零并触发像素断言。 [深度声明](../../../../Engine/Renderer/Paths/Forward.cpp#L1139) · [共享比较](../../../../Shaders/Shadow/Visibility.glsl) · [CSM](../../../../build/d09-t04/final/csm-Release.log) · [接收样点](../../../../build/d09-t04/final/offset-Release.log) · [置零对照](../../../../build/d09-t04/final/offset-zero-Release.log) |
| `punctual-lighting`、`static-forward-rendering` | 生成行直接配对灯与阴影组，并按灯对直接项应用可见性。Release 的 Direct 与带环境的 Physical 各比对 76,674 个样点，与固定场景逐灯隔离结果、Unlit、环境和自发光相符。复用了槽位代替换及失败前缀用例。普通受光与诊断的法线来源通过共享几何入口核查，原生偏移的定向样点使用固定几何法线。 [灯行打包](../../../../Engine/Renderer/Draw/StaticDraw.cpp#L944) · [逐灯查询](../../../../Shaders/Lighting/Punctual.glsl#L73) · [法线来源](../../../../Shaders/Geometry/SurfaceFrame.glsl) · [HDR 与 IBL](../../../../build/d09-t04/final/checks-Release.log) |
| `shader-build`、`opengl-shader-pipelines` | 两种配置各 11 个 `StaticForward` ABI 角色核对通过。图形数组声明、反射和非零起始层的 D32 原生取样吻合，原生数组异格式及越界请求被拒。IR 的二维替代被拒，Compute 采样数组在接口生成时被拒，合法 `image2d-array` 仍能生成及打包。 [Shader 生成](../../../../Engine/Shader/ShaderSchema.cpp) · [OpenGL 视图](../../../../RHI/OpenGL/Bindings.cpp) · [Release ABI](../../../../build/d09-t04/final/shader-abi-Release.log) · [Debug ABI](../../../../build/d09-t04/final/shader-abi-Debug.log) · [原生数组](../../../../build/d09-t04/final/array-Release.log) · [维度拒绝](../../../../build/d09-t04/final/wrong-shape.log) · [Compute 拒绝](../../../../build/d09-t04/final/compute-rejected.log) · [Compute 对照](../../../../build/d09-t04/final/compute-storage.log) |
| `render-graph-execution`、`render-frame-statistics` | 无消费者时裁剪到零阴影 Pass 和零瞬态分配，独立读回实际提交后可映射 D32。接受后下一轮复用 11 层，超范围图绑定只形成失败前缀，接受事实按 View 及自身票据保留。两种配置的双 View 回放都观察到主路 6、离屏 5 的 `InvalidExtent`，离屏恢复后接受并交付零消费。 [组图与提交](../../../../Runtime/Frame.cpp) · [统计快照](../../../../Runtime/FrameStatistics.hpp) · [Release 双 View](../../../../build/d09-t04/final/view-stats-Release.log) · [Debug 双 View](../../../../build/d09-t04/final/view-stats-Debug.log) |

对照了 9 份增量规格的 18 条要求和 47 个场景，核查改动的生产、消费、拒绝和接受合同。原有兼容场景按已验收的记录与直接调用点核对，定向回放覆盖改动风险。Debug 和 Release 正式构建与隐藏启动通过，各 17 个产品 Shader 包摘要均与 B 记录相同。完整逐灯 HDR/IBL 回放取自 Release，Debug 原生采样覆盖数组、图裁剪、读回及双 View。三档性能采用本工作包的原始同口径测量。 [正式构建与启动](../../../../build/d09-t04/final/build-final.log) · [回放入口](../../../../build/d09-t04/final/CheckFinal.ps1) · [产物及结果身份](../../../../build/d09-t04/final/native-verify.json) · [同步前严格校验](../../../../build/d09-t04/final/pre-sync-validation.json) · [三档测量](#a-performance)

<a id="spec-sync"></a>
## 规范同步

9 份增量已合并到对应的主规格，新增 6 条要求、修改 12 条要求，新增 14 个场景。同步保留各主规格的 Purpose、其他要求、未被修改的场景与原有顺序。逐项核验合并结果后，OpenSpec 1.13.2 严格校验通过 42/42 份主规格，当前 change 同步后也通过严格校验。 [逐项合并核对](../../../../build/d09-t04/final/spec-sync.json) · [主规格校验](../../../../build/d09-t04/final/main-validation.json) · [同步后 change 校验](../../../../build/d09-t04/final/post-sync-change-validation.json)

<a id="archive-closeout"></a>
## 归档验收

D09-T04 已按 OpenSpec 1.13.2 归档，17 项任务全部完成。B 签收问题单按原字节留存，归档与链接核对完成后按项目模板复位。D09-T05 前置已满足，待立项。 [归档回执](../../../../build/d09-t04/final/archive.json) · [归档核对](../../../../build/d09-t04/final/archive-audit.json) · [D09-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05)
