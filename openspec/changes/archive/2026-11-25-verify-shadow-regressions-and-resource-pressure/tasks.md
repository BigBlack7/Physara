# Tasks

## 1. 固定输入与现有证据

- [x] 1.1 确认 D09-T01 至 T04 的合同与前置归档，登记当前正式源码、构建配置、Shader 包、可执行文件、实际设备和驱动。将同身份已满足的检查逐项对应到 [V1–V6](design.md#verification-matrix)，组合覆盖或时序缺口转入当前回放，避免将历史结果计作当前实测。
- [x] 1.2 在 `build/d09-t05/` 固定单 View 质量场景及共享资产的双 View 压力序列。登记标样的独立预期、生成参数、许可、哈希、加载态及编辑顺序。以正式 Runtime/Graph 取得一帧正常接受并匹配实际 Shader、View、票据和读回，确认入口可达。

## 2. 受光质量回归

- [x] 2.1 完成 V1。用跨吸附格及非零过渡带的相机序列核对尺度、整 texel 对齐和过渡权重。读回 D32 与匹配的受光/诊断标样，区分薄片、MASK、负缩放、不同实例和画外 caster。在接收区仍有效时移除该层最后一个 caster，确认层清除及屏内恢复。
- [x] 2.2 完成 V2。以旋转聚光锥边及点光六面标样核对 D32、选面、同轴幅方向和三面角的可见性。按实际地址验证扩展面覆盖和有限阴影距离，保留一处覆盖外仍有直接光的对照。
- [x] 2.3 完成 V3。按独立预期验证常规深度、等深、混合核及接收偏移。使用接受的 HDR 与匹配诊断证明普通 Direct 和 Physical 使用正确的逐灯遮挡，环境、自发光及 Unlit 用固定标样核对。登记符合复用条件的双尺度偏移和置零失败证据，缺失时补其最小回放。

## 3. 图访问与资源压力

- [x] 3.1 完成 V4。记录 8→16 MiB 的点光整组拒绝和恢复，以及更多有效局部灯争用时的准入结果。用原生 HDR 证实代表性无分配灯继续照明。逐层匹配当前资源版本的写、导入及采样，对账图活性、无消费控制与显式 D32 读回。
- [x] 3.2 完成 V5 的区域换代、Off/On 及 resize 回放。旧新灯及深度标样可辨，验证相容区域换代且新输出可接受。记录无效附加 View 保留自身票据及统计、主 View 独立推进、附加 View 恢复后零阴影与再启用的结果。
- [x] 3.3 完成 V5 的 [在途时序](design.md#in-flight-replay)。对照实际 Pending 和完成的同一最后使用票据，核对旧深度和新输出、受保护物理槽的可用性、池上限与真实删除/复用。尺寸往返后验证容量收敛及可继续渲染，留存实际命令、观察位置及是否发生背压。

## 4. Forward 参考与实测成本

- [x] 4.1 完成 V6 的画质记录。以冻结的同一加载态取得 Off、Hard 512、PCF 512、PCF 256 的匹配 HDR、深度及标样。登记有效档位和灯组、容差、格式、覆盖率及图像差，给出 D10-T05、D11-T05 重建同输入所需的参数和预期。
- [x] 4.2 完成 V6 的成本测量。按 [测量协议](design.md#benchmark-protocol) 交付各档冷帧和暖帧的分布、接受/缺失数据、压力转折及实发计数。用匹配的图与输出确认空闲和 Hard→PCF 复用，计划字节、接受 D32 字节、池和原生工作量各有来源。阶段门槛有定量证据后结束采样。

## 5. A 自检与独立 B 闭环

- [x] 5.1 A 核对 V1–V6 与 D09 出口逐项匹配，记录实际命令、输入身份、结果、复用项、失败和未覆盖。必要修复限于可重现的已有合同偏差，对受影响消费者做定向自检。在 [B 签收问题单](../../../../build/d09-t05/final/review-signed.md) 登记交接，留待维护者调度独立 B。
- [x] 5.2 B 独立核对取证身份、代表性反例、在途事件顺序及画质与测量样本的一致性，记录稳定 CR 编号。若涉及正式第一方代码，按 [注释与审查约定](../../../../AGENTS.md#review-loop) 补齐并核对本轮涉及的完整文件的注释。见[首轮审查](#b-first-review)。
- [x] 5.3 A 针对 B 反馈修正并自检，用复现和定向回归证明整改，逐项登记变更及新的输入身份。若本轮无整改，记录 B 审查结论及复核依据。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2)
- [x] 5.4 B 在匹配最终身份的输入上复核整改与注释，逐票据验证 V6 GPU 样本和计时分布，CR-001 至 CR-007 全部关闭，独立签收当前版本。[首次复核](#b-recheck-1) · [最终签收](#b-recheck-2)

## 6. 签收版验收与归档

- [x] 6.1 A 在 B 签收版上执行最终 OpenSpec verify 和真实 OpenGL 复核，至少观察组合场景一次图准入及在途换代。对账对应的读回、完成、统计及测量身份，复用匹配证据并补输入变动或欠缺的检查。记录 D09 出口结果，将已验收的复用参考及复现参数落入 `Docs/Performance/` 并核对后续入口。 [最终验证](#final-verification)
- [x] 6.2 核对最终规格同步范围。当前 `skip_specs: true` 以严格校验和主规格合同核对完成 sync 判定。归档 change 并验证链接及证据留存，更新 D09-T05、D09 出口和 D10 前置状态，然后重置本轮问题单。

<a id="a-inputs"></a>
## A 输入与证据边界

基准 `v2.0` 的 `5b69ac039d03dabbe41373109bb52738b05ee5f8` 含 D09-T04 归档验收。工作区 50 项签收产品输入及 45 项 Release 构建产物逐字节一致，其中包含 17 个已验证产品 Shader 包。实际探针使用 Windows x64、MSVC Release、SDK 10.0.26100.0 和 Intel RaptorLake-S Mobile OpenGL 4.6.0 `32.0.101.6129`。第一方许可资产按原文件内容拷贝，派生夹具由脚本和正式 v1 文档恢复。 [前置签收记录](../2026-11-25-add-hard-pcf-shadow-visibility/tasks.md#final-verification) · [本轮输入清单](../../../../build/d09-t05/a/baseline-inputs.json) · [夹具生成](../../../../build/d09-t05/a/Prepare.py)

| 验收组 | 已核对的前置证据与本轮增量 |
|---|---|
| V1 | CPU 分割、吸附及原生过渡、镜像、双面与 MASK 已重放。本轮追加最后一个画外 caster 移除后的清除及恢复。 [CPU](../../../../build/d09-t05/a/csm-cpu.log) · [原生过渡](../../../../build/d09-t05/a/prior-csm.log) |
| V2 | 局部有限范围和窄锥精度的 CPU 结果已重放。本轮追加旋转聚光的像素判据、点光六层及无 range 直接光。 [CPU](../../../../build/d09-t05/a/cpu.log) |
| V3 | 等深和 `3 × 3` 比较及非零起始层实际采样已重放。签收的双尺度偏移与负控、逐灯 HDR/IBL 文件身份匹配。本地首次整套 HDR 重放等候 IBL 超时，留存首次结果并单独调度复核。 [D32 比较](../../../../build/d09-t05/a/prior-array.log) · [首次完整重放](../../../../build/d09-t05/a/prior-lighting.log) |
| V4 / V5 | 签收版的 8/16 MiB、图裁剪、前缀、双 View 统计与后继读回提供局部判据。本轮在派生场景核对实际大灯数、区域和尺寸替换，并要求同一原生 fence 的在途观察。 |
| V6 | 旧三档数据提供取样口径。当前固定 Physical/IBL 场景、四档画质与实测数据分别取自本轮。 |

<a id="a-regressions"></a>
## A 原生回归与资源压力

- V1 · 原生级联诊断核对 64,124 个过渡样点，810 个样点的相邻两级结果有区别。实际读取的镜像、薄片双面和 MASK 孔各有标样。独立移除控制使两级带接收区的 D32 各 65,536 个 texel 清为 1，1,907 个屏内样点从遮挡恢复可见。 [过渡](../../../../build/d09-t05/a/prior-csm.log) · [最后一个 caster](../../../../build/d09-t05/a/a1-last-caster.log)
- V2 · 点光按 +X、−X、+Y、−Y、+Z、−Z 顺序读取六层，实际非清除 texel 分别为 7,134、3,312、3,496、1,519、0、6,417。边缝两侧、轴幅相等及三面角按各自面取样。旋转聚光锥内有 146 个遮挡和 650 个明亮样点，锥外 780 个样点返回中性。无 range 点光在有限阴影距离外仍有 1,452 个直接光样点。 [局部回放](../../../../build/d09-t05/a/a1-local.log)
- V3 · 实际指定比较核 Hard 为 0、PCF 为 0.555176，等深为 1。双尺度 CSM 和 spot 偏移正例各取得 Hard `[1, 0, 1, 1]`、PCF `[0.666504, 0, 0.666504, 1]`，置零包产生四组全零并触发独立像素断言。首次整套回放在 20 秒内未取得匹配 IBL 帧。将本地探针的等待窗口改为 120 秒后，Direct 与含 IBL 的 Physical 各核对 76,674 个接收样点，实际受光、诊断、Unlit、自发光与环境对照通过。 [逐灯 HDR](../../../../build/d09-t05/a/a1-lighting.log) · [双尺度](../../../../build/d09-t05/a/offset.log) · [置零负控](../../../../build/d09-t05/a/offset-zero.log)
- V4 · 512 texel 下从 8 MiB 的五层过渡到 16 MiB 的十一层，1,832 个点光样点出现恢复的遮挡。51 盏可见灯争用，最终四级方向光和十二盏聚光占十六层，38 盏拒绝。改变一盏未分配点光的强度，30,405 个蓝通道样点仍响应，深度只读复用十六层。存活计划逐层核对 Clear、产出版本、整组采样和显式 D32 复制。同身份探针重放得到无消费零 Pass/零字节，独立读回有实际票据及 D32 内容。 [压力与实际图](../../../../build/d09-t05/a/a1-pressure.log) · [活性控制](../../../../build/d09-t05/a/prior-view.log)
- V5 · 聚光复用逻辑 region 时实体代、分配代、内容代、D32 和 HDR 各自换代。附加 View 尺寸无效时保留票据 13，主 View 到 14，恢复到 400 × 300 的 Off 帧零深度消费，开启后接受十一层。五次尺寸往返后，双 View 池稳定在 125,771,524 B，末次空闲 100,904,704 B，保持 30 个物化对象累计数并持续命中。 [双 View 与尺寸回放](../../../../build/d09-t05/a/a1-pressure.log)
- V5 · 真实图生产同一十一层数组，显式 staging 为最后用途。仅一个 11 MiB 槽的原生压力试验中，票据 29 在竞争准入前后均得 `GL_TIMEOUT_EXPIRED`，在途 11,534,336 B，外部持有与预留均为零，返回 `PoolInFlight`。实际完成后票据 30 复用原生纹理 35，旧、新 spot 层有 14,336 个不同像素。最终为一个可复用的完成空闲槽。 [完成与池记录](../../../../build/d09-t05/a/a1-pressure.log) · [旧层 D32](../../../../build/d09-t05/a/retired-old-spot.d32) · [新层 D32](../../../../build/d09-t05/a/retired-new-spot.d32)

<a id="a-forward-reference"></a>
## A Forward 画质与成本参考

本节保留 A 初测和匹配的画质参考。当前 V6 的完整 GPU 样本及分布见[第二轮整改](#a-correction-2)。

`ForwardShadow.scene.json` 的 SHA-256 为 `0450f90d0e234b57a6c94426598767afa55e5557bafe174077cbdbf47f74b76c`。使用冻结加载态 revision 1、640 × 480、Physical Lighting、None AA、1.0 render scale、正交 8 × 6 m、相机位置 `(0,0,8)`、f/4、1/16 s、ISO 100 和 0 EV 补偿。参考 EXR 的倍率为 16，启用环境照明和天空。一个方向光、一个 spot 和一个无 range 的 point 在 16 MiB 下共用四级、一层和六层，比较时保持灯组和相机相同。窗体回执的有效交换区间为 0。 [固定场景](../../../../build/d09-t05/a/input/Assets/Scenes/ForwardShadow.scene.json) · [重建脚本](../../../../build/d09-t05/a/Prepare.py) · [画质帧身份](../../../../build/d09-t05/a/quality/frames.csv)

四档主 Reverse-Z D32 完全一致，274,168 个地面像素通过独立投影深度判定。11 个固定 ROI 的明亮、MASK 孔和遮挡分类均匹配几何预期，验收按 `2e-6` 深度公差、`3e-4` 相同亮区公差与 `5e-4` 阴影差阈值。处在级联过渡带的画外、接缝及角点样块按各自通道验证。Hard 512 与 PCF 512 的全部十一层 D32 逐字节相同，3,813 个地面像素的 HDR 最大通道差超过 `3e-4`。PCF 512 与 PCF 256 有 6,751 个这样的像素。后续 D10-T05、D11-T05 可用相同档位及 ROI 比较预曝光 RGBA16F、主深度和实际阴影工作量。 [图像与 ROI 数值](../../../../build/d09-t05/a/quality-summary.json) · [读回数值核对](../../../../build/d09-t05/a/Report.py)

每档三轮，每轮暖机六帧后采 24 个已接受帧。CPU 工作包含 UI、准备、构图、编译及执行，时间均为毫秒，GPU 按实际就绪的原票据配对。记录 72 个暖帧的场景输出和实际交换结果。 [逐票据数据](../../../../build/d09-t05/a/timing.csv) · [各区间分布与缺失](../../../../build/d09-t05/a/timing-summary.json)

| 有效档位 | CPU median / p95 | GPU median / p95 | GPU Ready | 槽等待 median / p95 | 交换等待 median / p95 | 接受 D32 B |
|---|---:|---:|---:|---:|---:|---:|
| Off | 3.019 / 5.364 | 1.771 / 3.998 | 72 | 0.017 / 0.025 | 0.051 / 0.077 | 0 |
| Hard 512 | 4.823 / 6.336 | 3.153 / 4.721 | 71 | 0.018 / 0.041 | 0.057 / 0.108 | 11,534,336 |
| PCF 512 | 5.097 / 7.105 | 3.443 / 4.827 | 72 | 0.020 / 0.046 | 0.059 / 0.135 | 11,534,336 |
| PCF 256 | 4.760 / 6.494 | 3.164 / 4.826 | 72 | 0.018 / 0.039 | 0.056 / 0.525 | 2,883,584 |

暖帧各为 13 次原生 Draw、6 次上传共 40,454 B，Off 为 51 次绑定，各开启档为 56 次。各档暖帧没有新原生纹理或 Buffer。各开启档首次接受生产十一层、28 个阴影批次和整图 41 次原生 Draw，原生创建与计时见冷帧原始样本。票据 974 从暖 Hard 512 切到 PCF 512 时重用十一层且零深度重绘。签收前的数值以本机实际统计区间为适用范围。Hard 512 缺一个暖 GPU Ready，Off 与 PCF 512 各缺首次接受 GPU 结果。 [实测入口与结果](../../../../build/d09-t05/a/a1-timing.log) · [统计汇总](../../../../build/d09-t05/a/timing-summary.json)

<a id="a-handoff"></a>
## A 交接

A 核对了六组验收与 D09 出口所需的按灯逐项覆盖、预算降级、图权限和退休。使用 `Compile.ps1` 生成并链接实际探针。`RunStage.ps1 -Modes @('last-caster','local','pressure','quality','timing','lighting') -Run 'a1'` 的六项均返回 0。由 `Reuse.ps1` 重放同身份局部证据和偏移负控，`python -B build/d09-t05/a/Report.py all` 核对原图与计时同一加载态的身份。读取 D32 时使用图授权的测试 PBO，预曝光 HDR 走现有 staging。采样窗口关闭详细图观测及像素读回。 [编译入口](../../../../build/d09-t05/a/Compile.ps1) · [串行回放](../../../../build/d09-t05/a/RunStage.ps1) · [实际回执](../../../../build/d09-t05/a/a1-receipts.json) · [前置回放记录](../../../../build/d09-t05/a/replay-extra-receipts.json)

实现与回放代码在工作包临时目录，正式产品库、Shader、架构合同和主规格沿用 D09-T04 签收输入。前置历史中的 Debug 局部原生证据保留原身份，本轮完整验证范围为当前 Release 及上述实际设备。原生结果与文件 SHA-256 已登记。独立复跑可在与 A 输出分开的本地工作区完成，保持本轮原始结果供核对。 [B 签收问题单](../../../../build/d09-t05/final/review-signed.md) · [交接清单](../../../../build/d09-t05/a/handoff.json)

<a id="b-first-review"></a>
## B 首轮审查

2026-11-25，B 核实了上述基准及 A 交接清单中的源码、产物、夹具和证据哈希。B 复跑 Release CSM CPU、局部灯 CPU 和原生 `local`，三项通过。用原始 D32、HDR 与 CSV 重算所得画质和计时摘要与 A 的记录一致。完整压力序列、IBL 回放及计时窗口引用 A 的实际运行。正式第一方源码无本轮变更，B 核对了计划、查询、图访问、逐 View 接受、池退休及统计的既有说明，源码注释编辑为零。

CR-001 至 CR-006 待 A 整改。新旧 spot 图对调后原判据仍通过；点光的面身份、Y/Z 并列和半径外中性需要独立标样；提前完成的 fence 需重新取样。首次 51 灯重建和计时期间各次尝试也需要按实际帧记录。5.2 记录首轮审查完成，5.4 最终签收仍待复核。各项反例、优先级及定向验证要求见[B 签收问题单](../../../../build/d09-t05/final/review-signed.md)。

<a id="a-correction-1"></a>
## A 首轮整改

在 `build/d09-t05/a-correction-1/` 定向修改回放。重新核对基准 `5b69ac039d03dabbe41373109bb52738b05ee5f8` 的 50 项签收源码和 45 项 Release 产品输入，复用原夹具和匹配的 V1、V3、画质原图及局部图活性证据。当前探针直接链接同一组产品库。原 A 结果与 B 首轮问题单按原字节留存，留存问题单的相对链接以 `Docs/CodeReview.md` 为基准。 [首轮问题单留存](../../../../build/d09-t05/a-correction-1/review-b-first.md) · [原 A 清单](../../../../build/d09-t05/a/handoff.json)

| CR | 改动与定向复核 |
|---|---|
| CR-001 | 按灯、挡板与地板的相对位置确定两枚独占接收标样。旧、新接受帧各比较自己的 Off 参照，实际证明旧遮挡释放和新遮挡建立。原生图退休后，旧层 `(345,256)` 写入、新层该处清除，`(438,256)` 恰好相反。按各提交留存两层，交换顺序时同一深度判据失败。 [回放](../../../../build/d09-t05/a-correction-1/verify-pressure.log) · [旧 D32](../../../../build/d09-t05/a-correction-1/retired-old-spot.d32) · [新 D32](../../../../build/d09-t05/a-correction-1/retired-new-spot.d32) |
| CR-002 | +Y/−Y 各使用离轴写入和镜像清除标样，旧层交换控制失败。最初平板上的数学并列在实际插值后分到 −Z，保留该次失败。对称接收板使 Y 与 −Z 随图元等值插值，正式包显示全可见 +Y，只改 `>=` 为 `>` 的测试包显示 −Z，在同一断言退出 1。 [首个标样](../../../../build/d09-t05/a-correction-1/trial-local.log) · [正式输出](../../../../build/d09-t05/a-correction-1/verify-local.log) · [负控输出](../../../../build/d09-t05/a-correction-1/verify-local-yz-control.log) |
| CR-003 | 固定点 `(277,120)` 的射线穿过已实际写入 D32 的挡板，轴深仍在投影内，径向距离越出 4 m。点光诊断返回中性，同强度 On/Off 的蓝通道相等。置零光强得到独立的直接项对照。 [半径外结果](../../../../build/d09-t05/a-correction-1/verify-local.log) |
| CR-004 | 到达准入前或从准入到第二次轮询间已完成的样本，按真实完成收拢后在最多六次实际生产内重试。首轮受控错过票据 32，同一 fence 的真实完成值为 `0x911a`。票据 33 前后均为 `0x911b`，`PoolInFlight` 未物化槽，完成后票据 34 安全复用纹理 35。 [时序](../../../../build/d09-t05/a-correction-1/verify-pressure.log) |
| CR-005 | 首次 51 灯的票据 9 分别记录两路各 16 层与 21 批次的冷实际生产，整图 58 Draw、235 次绑定、107 次上传（2,436,234 B）、2 次纹理创建。后继票据 10 两路各复用 16 层，整图 16 Draw、65 次绑定、13 次上传（2,298,170 B）、零纹理创建。两次图池计数均显示 69,808,132 B 峰值。 [逐 View 及整图记录](../../../../build/d09-t05/a-correction-1/verify-pressure.log) |
| CR-006 | 按档位、冷启动及过滤切换、预热和采样记录全部真实返回。1,729 次中 `Rendered` 1,724，`SafeUi` 5，取消、上传等待及错误各 0。已渲染中的 381 次匹配请求，另 1,343 次在 Off 冷启动阶段尚未匹配。仅用身份匹配的接受暖帧重新计算四档各 72 帧的 CPU/GPU 分布。GPU Ready 为 55/47/49/49，其余保留缺值。 [全部尝试](../../../../build/d09-t05/a-correction-1/attempts.csv) · [接受样本](../../../../build/d09-t05/a-correction-1/timing.csv) · [独立对账](../../../../build/d09-t05/a-correction-1/timing-reconciled.json) |

`Run.ps1 -Build -Tag 'verify' -Modes @('local','local-yz-control','pressure')` 及计时入口 `Run.ps1 -Tag 'verify-timing' -Modes @('timing')` 在当前 Intel RaptorLake-S Mobile 的 OpenGL Release 匹配预期退出码。`python -B build/d09-t05/a-correction-1/Reconcile.py` 通过。A 自检范围为本轮修正涉及的回放与身份对账。 [回放入口](../../../../build/d09-t05/a-correction-1/Run.ps1) · [核对回执](../../../../build/d09-t05/a-correction-1/verify-receipts.json) · [计时回执](../../../../build/d09-t05/a-correction-1/verify-timing-receipts.json) · [整改交接清单](../../../../build/d09-t05/a-correction-1/handoff.json)

<a id="b-recheck-1"></a>
## B 首次整改复核

B 按 A 整改清单 `a40bb9fd0f4ba8af18440d0d26830b0056361abf286b45ea2f731794422c4318` 核对 `v2.0` 基准 `5b69ac039d03dabbe41373109bb52738b05ee5f8` 的产品源码、Release 产物、回放输入及证据。正式源码与构建输入沿用签收版本，本轮正式源码注释编辑为零。B 以同一交接探针独立运行局部灯、Y/Z 负控、双 View 压力和计时，退出码依次为 0、1、0、0。负控只在预期的并列面色断言退出。设备为 Intel RaptorLake-S Mobile OpenGL 4.6.0，驱动 32.0.101.6129。[独立回放与按票据核对](../../../../build/d09-t05/b-recheck-1/recheck.json)

- **CR-001 至 CR-003 已关闭。** 普通换灯和在途复用的原始 D32 在各自几何标样上核对通过，交换旧新读回时都失败。+Y/−Y 离轴标样可识别镜像及错层；Y/Z 并列在产品上选 +Y，在负控上选 −Z。轴深仍在投影内、径向超过 4 m 的点光标样取中性，阴影 On/Off 直接光相同，强度归零后下降。[压力回放](../../../../build/d09-t05/b-recheck-1/pressure.log) · [局部灯](../../../../build/d09-t05/b-recheck-1/local.log) · [Y/Z 负控](../../../../build/d09-t05/b-recheck-1/local-yz-control.log)
- **CR-004 至 CR-006 已关闭。** 错过首个 fence 后重新取得同票据双侧 Pending，完成后安全复用。51 灯双 View 的冷重建与暖导入分别归属图 9、10。A 与 B 的四档计时按阶段均能对账，每档 72 个匹配暖帧；CPU 区间和场景/质量身份已核对。[冷暖及退休](../../../../build/d09-t05/b-recheck-1/pressure.log) · [B 计时](../../../../build/d09-t05/b-recheck-1/timing.log) · [分布与尝试对账](../../../../build/d09-t05/b-recheck-1/recheck.json)
- **CR-007 [P2] 待 A 整改。** B 在同一原生计时运行中比较聚合前的有效 Timer 与 A 的面板快照采集；四档各有 72 个匹配 Ready 暖帧，CSV 实收 51/49/49/49，共漏 90 个。V6 GPU 分布待按原票据更新；5.4、6.1、6.2 保持待办。[同次运行反例](../../../../build/d09-t05/b-recheck-1/timer-check/comparison.json) · [B 签收问题单](../../../../build/d09-t05/final/review-signed.md)

<a id="a-correction-2"></a>
## A 第二轮整改与 V6 计时参考

CR-007 的定向回放位于 `build/d09-t05/a-correction-2/`。本地 `FrameStatistics.cpp` 副本在每轮 `Poll` 开始时登记轮询序号，在查询进入有来源的终态时复制原提交身份和结果。采集使用定长内存，查询、可用性判断、回收和面板聚合沿正式流程。窗口结束后将原始记录按提交、Scene、View、内容与文档版本、Shader 代及输出配置同接受帧配对。有效结果保留秒数。不可用、跳过、失败和窗口结束时仍未就绪分别保留状态。 [Runtime 测试副本](../../../../build/d09-t05/a-correction-2/replay/ObservedStatistics.cpp) · [收集与序列化](../../../../build/d09-t05/a-correction-2/replay/GpuTrace.inc)

仍用原 [固定画质场景](#a-forward-reference)、相同 50 项产品源码和 45 项 Release 产物。1,756 次实际尝试中有 1,751 次 `Rendered`、5 次 `SafeUi`，取消、上传等待和错误均为零。381 次匹配请求，1,370 次已渲染但尚未匹配。1,757 次正常轮询共记录 381 个有来源的 Ready，无查询失败。四个冷帧、一个暖过滤切换和 288 个暖采样帧的全部 293 条均取到原票据时长，当前采样缺测为零。 [原始查询终态](../../../../build/d09-t05/a-correction-2/gpu-resolutions.csv) · [计时 CSV](../../../../build/d09-t05/a-correction-2/timing.csv) · [尝试记录](../../../../build/d09-t05/a-correction-2/attempts.csv)

独立脚本逐票据核对来源、终态、轮询序号和秒数。第 1,396 轮同时解析 Off 的票据 1393、1394、1395，实收 `2.926612 / 2.894892 / 2.902432 ms`。同次运行有 70 次轮询含多个匹配暖帧。用原始结果重建每轮仅暴露最大票据的面板读法，104 个已经 Ready 的暖帧从未出现在该快照中。 [同次运行对账及旧采集对照](../../../../build/d09-t05/a-correction-2/timing-reconciled.json)

| 有效档位 | CPU 工作 median / p95（ms） | GPU median / p95（ms） | 暖 GPU Ready | 暖 GPU 缺测 |
|---|---:|---:|---:|---:|
| Off | 1.707 / 2.396 | 2.095 / 5.096 | 72 | 0 |
| Hard 512 | 1.936 / 2.521 | 3.541 / 5.286 | 72 | 0 |
| PCF 512 | 1.921 / 2.618 | 3.712 / 5.487 | 72 | 0 |
| PCF 256 | 1.901 / 2.651 | 3.836 / 5.506 | 72 | 0 |

执行 `RunTiming.ps1 -Tag 'verify'` 和 `python -B build/d09-t05/a-correction-2/Reconcile.py`。范围为当前 Intel RaptorLake-S Mobile OpenGL 4.6.0 驱动 `32.0.101.6129` 的 Release 定向回放。V1–V5 和冻结画质按匹配身份复用，CR-001 至 CR-006 沿用 B 已关闭的记录。B 首次复核问题单按原字节留存，其中相对链接以 `Docs/CodeReview.md` 为基准。 [原生复跑入口](../../../../build/d09-t05/a-correction-2/RunTiming.ps1) · [运行回执](../../../../build/d09-t05/a-correction-2/verify-receipt.json) · [B 首次复核原件](../../../../build/d09-t05/a-correction-2/review-b-recheck-1.md) · [第二轮交接清单](../../../../build/d09-t05/a-correction-2/handoff.json)

<a id="b-recheck-2"></a>
## B 第二次整改复核与签收

2026-11-25，B 签收 `v2.0` 基准 `5b69ac039d03dabbe41373109bb52738b05ee5f8` 对应的第二轮交接。交接清单 SHA-256 为 `65807ccf247c7047e7bd09b050064251365df228b4bb87b0864895936b764b59`，实际 Release 探针 SHA-256 为 `6997d2b9d24fea69894212e81f3ca2b69ab2ede5aa66ebe24200fe7db79e5fa8`。50 项产品源码和 45 项 Release 输入与此前签收输入一致。B 核对 26 项回放/夹具输入及原始证据，核实工作包的观测副本仅在正式非阻塞轮询中登记轮次与有来源的查询终态，窗口结束后才配对和写盘。[A 交接清单](../../../../build/d09-t05/a-correction-2/handoff.json) · [B 核对结果](../../../../build/d09-t05/b-recheck-2/recheck.json)

B 用交接探针和相同 Shader 包、冻结夹具独立运行 `Native-Release.exe timing`，退出码 0。每档三轮、每轮 24 个匹配暖帧，四档共 288 个；4 个冷帧和 1 个 Hard→PCF 切换帧单独核对。A 与 B 两次回放的 293 条采样记录均能逐票据匹配原始 Ready 事件的身份、轮次和时长。B 回放有 58 次轮询一次取得多条暖帧结果，包括轮询 1371 的票据 1369、1370。对同一数据模拟旧快照筛法会漏 78 个暖帧；将 A 的票据 1393 改回缺测时，原始终态对账按预期拒绝。B 从 A 的 CSV 重算 CPU/GPU median、p95，四档均与[第二轮计时参考](#a-correction-2)一致。[B 原生运行](../../../../build/d09-t05/b-recheck-2/timing.log) · [B 原始查询](../../../../build/d09-t05/b-recheck-2/build/d09-t05/a-correction-2/gpu-resolutions.csv) · [B 接受样本](../../../../build/d09-t05/b-recheck-2/build/d09-t05/a-correction-2/timing.csv) · [独立对账](../../../../build/d09-t05/b-recheck-2/recheck.json)

CR-007 关闭，CR-001 至 CR-006 沿用首次复核结论。V1–V5 与全幅画质使用已核对的同输入证据；本轮新增原生复跑覆盖同一 Intel OpenGL 4.6.0 驱动 32.0.101.6129 下的 Release V6。实际回放全部取到 Ready，其他查询终态依据代码核对。本轮核对了工作包计时注释，正式源码及注释编辑为零。B 已完成 5.4，A 接手 6.1、6.2。[B 签收问题单](../../../../build/d09-t05/final/review-signed.md)

<a id="final-verification"></a>
## A 最终验证与 D09 阶段出口

核对 B 签收的 `v2.0@5b69ac039d03dabbe41373109bb52738b05ee5f8`。50 项源码、45 项 Release 输入、7 项签收文档及探针和历史证据身份匹配，暂存区为空。将实际签收的 Release 探针、17 个 Shader 包和 7 个夹具输入复制到独立回放目录。正式 `Physara.exe --hidden --poll-limit=15` 与签收探针 `pressure` 均退出 0。 [签收输入和原件](../../../../build/d09-t05/final/signed-inputs.json) · [回放入口](../../../../build/d09-t05/final/Run.ps1) · [原生回执](../../../../build/d09-t05/final/native-verify.json)

| 验收组 | 签收版判定 |
|---|---|
| V1 · CSM 与静态覆盖 | 按同输入复用级联吸附、过渡带、画外投射、MASK 孔、薄片双面、镜像和实例原生证据。最后一个 caster 移除后两层清为 1，1,907 个接收像素恢复。 [A 回归](#a-regressions) · [B 核对](#b-recheck-1) |
| V2–V3 · 局部灯及共享可见性 | 复用 B 就六面身份、接缝、并列顺序及有限半径的复核。保留反转 Y/Z 负控和接收偏移置零负控，Hard/PCF 的比较核与常规深度和逐灯 HDR 对照相符。 [首轮整改](#a-correction-1) · [B 核对](#b-recheck-1) |
| V4 · 图准入及访问 | 重放确认 8 MiB 接受五层、16 MiB 接受十一层。51 灯争用时 38 盏的阴影未获分配，代表灯仍参与直接照明。票据 9 两路各冷生产 16 层，整图 58 次 Draw 和 2 次纹理创建。票据 10 两路各复用 16 层，整图 16 次 Draw、零纹理创建。无消费者裁剪和显式深度读回沿用匹配证据。 [签收版压力回放](../../../../build/d09-t05/final/pressure-Release.log) · [活性控制](../../../../build/d09-t05/a/prior-view.log) |
| V5 · 换代、停用、尺寸与退休 | 回放中的无效附加 View 保留票据 18，主 View 推进到 19。恢复尺寸后 Off 接受零阴影，On 接受十一层。受控首次完成后重试，票据 33 的同一 fence 在准入前后均为 Pending，池以 `PoolInFlight` 拒绝。设备确认完成后票据 34 复用纹理 35。换灯和在途复用的四份 D32 均与首轮整改读回逐字节匹配。 [原生回执及读回身份](../../../../build/d09-t05/final/native-verify.json) |
| V6 · 冻结画质和成本 | 复核 A、B 各 293 条计时与原始 Ready 查询的票据、Scene/View、轮询和秒数，各含 288 个暖帧。复算签收 A 的四档 CPU/GPU median/p95 与发布值一致。沿用同冻结加载态的 11 个 ROI 和全幅证据。 [签收计时](#a-correction-2) · [可复用画质与成本参考](../../../../Docs/Performance/ForwardShadowReference.md) |

D09 阶段出口通过，覆盖当前 Windows x64 Release、Intel RaptorLake-S Mobile OpenGL 4.6.0 驱动 `32.0.101.6129` 的静态 Forward。最终补跑限于启动及组合图准入和在途换代。冻结图像、已签收的局部用例及 V6 数据按实际身份复用。设计符合正式加载、现有 Shader ABI、非阻塞计时和图/RHI 完成协议。本工作包以 `skip_specs: true` 验收已采纳的主规格。 [变更严格校验](../../../../build/d09-t05/final/validate-change.json) · [42 份主规格校验](../../../../build/d09-t05/final/validate-specs-before.json)

<a id="archive-closeout"></a>
## 归档与阶段推进

OpenSpec 1.13.2 按 `spec-driven` 完成归档，16 项任务全部完成。状态中规格 artifact 为 skipped，实际增量路径为空。归档前后 42 份主规格严格校验通过且哈希一致。D09 阶段出口通过，D10 当前工作包为 D10-T01，前置已满足，待立项。B 签收问题单按原字节留存，原件的相对链接以签收时 `Docs/CodeReview.md` 为基准。实时问题单已按模板复位。 [归档回执](../../../../build/d09-t05/final/archive.json) · [归档校验](../../../../build/d09-t05/final/archive-audit.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [签收原件](../../../../build/d09-t05/final/review-signed.md)
