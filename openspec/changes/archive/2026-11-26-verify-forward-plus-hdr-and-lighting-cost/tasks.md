# Tasks

## 1. 冻结输入并接通正式执行链

- [x] 1.1 A 核对 D09 参考和 D10-T01 至 D10-T04 的签收证据。登记本轮构建、相关暂存／未暂存差异及未跟踪输入、实际包及加载代、设备驱动、版本和命令。核对可复用的场景、许可、哈希和重建参数，写出每项复用的身份条件与本轮缺口。 [输入合同](design.md#frozen-inputs)
- [x] 1.2 A 在 `build/d10-t05/` 复用正式 Runtime/Renderer/Graph/RHI，取得两条路径各一帧真正接受且匹配配置的输出。核对 `workingColor` 和 D32 的图读回版本、完成票据、row pitch 与路径角色，并用一次当前请求与旧接受值不同的观测检验采样准入。留存身份、命令和最小读回结果。 [读回合同](design.md#hdr-readback)

## 2. 验证 HDR 及灯贡献完整性

- [x] 2.1 A 完成 V1。对 D09 参考的有效 PCF 512 与 IBL 固定档位取得当前正式两路径的 RGBA16F 和 D32。核对正常接受的阴影过滤、灯组、层、环境和曝光，按数值门槛输出全画布差分及照明、遮挡、MASK、天空标样。保留实际输入、指标和限制。 [验收规格](design.md#acceptance-spec)
- [x] 2.2 A 完成 V2。在相同冻结灯表上比较正常格、全局灯、混合 overflow 及无有效 cell 的产品 HDR。用实际查询选择和已上传的候选行核对有限点光、斜向 spot 锥与径向边界、无 range 点光／聚光及方向光。用移除一盏已知贡献灯的全灯对照验证差分可发现遗漏。留存标样、分支与灯行证据。 [候选观测](design.md#candidate-observation)
- [x] 2.3 A 完成 V3。取得无限远透视与零近平面正交的有效受光输出，验证有限分箱的近远界、留边有效矩形和真实空灯。针对远移与精度风险复用身份匹配的证据或补当前最小回放。保存 D32、HDR、分支、有效行数和未覆盖条件。 [相机与空灯](design.md#acceptance-spec)

## 3. 测量同画质的多灯成本

- [x] 3.1 A 接通 V4 的局部分箱计时、实际 Froxel 写笔数／字节与 CPU 上传区间，以及原生 `colorPass` GPU 区间。以一组已接受路径核对所有区间、所属 View、写读范围和原票据。采集侧用同轮多条计时返回核对聚合前数据完整性，记录查询缺测和区间边界。临时观测随工作包命令及输入登记。 [测量协议](design.md#benchmark-protocol)
- [x] 3.2 A 对 `0 / 32 / 256` 个可见局部灯及密集溢出控制先取得等质量量化结果，验证正式有效路径、几何覆盖、真实裁剪数和像素贡献。用独立的匹配原生回放记录 overflow 格比例、受光像素退路比例及每次退路的局部灯行数。确认暖帧的场景与画质身份对应计时组。 [V4 通过判据](design.md#acceptance-spec)
- [x] 3.3 A 完成交错暖帧采样，按实际原生返回逐票据对账并复算 median／p95 与分轮差，记录 CPU 准备、Froxel 上传、GPU 颜色及局部灯增量、整图和等待数据，保留实际操作计数。依据同画质时的成本和退路说明净变化及波动。登记有效样本、缺测与受影响的复测组，证据足够即结束采样。 [取样与对账](design.md#benchmark-protocol)

## 4. A 本地验收与交接

- [x] 4.1 A 按工作包的三项范围核对 V1–V4 判据，与真实构建和输入清单互证，补齐直接缺口。按 [证据记录](#evidence) 保存本地结果和限制，在 [签收问题单](../../../../build/d10-t05/final/review-signed.md) 记录待审身份和可重放入口。相关 B 栏与最终验收保持待处理。 [独立交接合同](../../../../AGENTS.md#review-loop)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核对正式入口、回放和实际接受身份，复核并重算代表性的 HDR、正常／退路、上传范围与异步时长分布。通读及完善本轮实际涉及的第一方文件注释，在问题单标明覆盖、稳定 CR 条目、证据和待验收条件。首审登记 CR-001 至 CR-005，待 A 整改。 [首审问题单](../../../../build/d10-t05/final/review-signed.md) · [审查规范](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 5.2 A 按 CR 编号整改，重跑能判别问题及受影响消费者的最小检查，保存源码／证据身份、结果和剩余限制，只确认 A 自检项。CR-001 至 CR-005 已自检，交 B 复核。 [首轮整改](#a-correction-1)
- [x] 5.3 B 独立复核最终差异、必要原生回放、证据配对和注释覆盖，关闭通过的条目。未通过则保留编号送回整改，满足全部判据后签收实际输入和可重用证据。CR-001 至 CR-005 已关闭。 [B 签收记录](#b-recheck-1)

<a id="final-verification"></a>
## 6. 签收版验证与 D10 出口

- [x] 6.1 A 在 B 签收的输入上执行最终 verify。核对计划、实际产物和签收记录，原生复验必要 HDR／退路、上传和区间归属，逐项记载重跑、匹配复用和未覆盖结果。涉及受审实施或测量输入的变动时先取得相应 B 复核。 [原生验收](#final-native-evidence)
- [x] 6.2 A 以已签收的质量和成本整理 `Docs/Performance/ForwardPlusReference.md`，七组分布、三个冻结输入摘要与 D09 深度计数均和最终复算一致。签收问题单原字节已留存。锁定的 OpenSpec 严格校验通过，`skip_specs: true` 对应零规格增量。 [双路径基准](../../../../Docs/Performance/ForwardPlusReference.md) · [签收问题单](../../../../build/d10-t05/final/review-signed.md)
- [x] 6.3 A 完成 OpenSpec 归档并核对归档回执、链接和归档后的规格状态。将实际支持范围和验收引用写入工作包、当前阶段及 D10 出口，满足条件后标记 D11 入口可用。归档及证据留存确认后依空模板重置当前问题单。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [问题单模板](../../../../Docs/CodeReview.md.tpl)

<a id="final-native-evidence"></a>
### 签收输入的最终验证

B 签收的基准提交、两处产品注释、A 交接及 B 原生复核资料相符。核对 333 个签收输入和 15 个 B 原生读回及票据文件。签收版 Release 正常隐藏启动并退出 0。隔离目录以同一签收回放程序重跑 `reference`、`local`、`local-map`、`local-bypass`，15 组新接受图像的 RGBA16F/D32 和签收数据逐字节一致。Froxel 读写范围、受光标样、D09 的 274,168 个地板样点、溢出及正常格实际灯行数通过复验。 [签收输入](../../../../build/d10-t05/final/signed-inputs.json) · [最终原生回执](../../../../build/d10-t05/final/native-verify.json)

已签收的相机边界、B 远移和规模画质及观测样本按身份复用。重算原有 504 个接受暖帧的 1,008 个 GPU 区间，七组分布与原结果一致，换标负例在工作量准入时被拒绝。画质与计时使用签收的 Windows x64 Release、Intel OpenGL 4.6.0 驱动 32.0.101.6129、640 × 480、None AA 和有效 VSync 0。本轮复算原有计时样本，独立 GPU 传输区间及其他设备、驱动或画质档位没有新测量。 [重跑和复用范围](../../../../build/d10-t05/final/replay-plan.json) · [逐票据复算](../../../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/timing-summary.json)

验证对应设计的 V1–V4，覆盖 HDR、候选完整性和成本归因，临时观测沿用签收的图执行与 Shader 合同。OpenSpec 的增量规格检查按 `skip_specs: true` 跳过，本包沿现有主规格验收。 [设计判据](design.md#acceptance-spec) · [严格校验](../../../../build/d10-t05/final/verify-change.json)

<a id="evidence"></a>
## 证据记录

实施时在本文件记录每项检查的命令、输入身份、实际结果、适用范围和必要的原始输出位置。复用项注明与本轮输入一致的证据及其判据。未覆盖项保留实际原因与受影响的验收项。签收记录含 B 确认的版本、CR 闭环、注释覆盖及最终验收结果。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md)

本轮复核以 [A 首轮整改](#a-correction-1) 为入口，下文首轮 A 记录保留原交接结果。

### A 输入与前置证据

本轮基准为 `v2.0@2e94c06064d32f1cfcf2000e6e1b1c7020ac3155`，产品源码与 D10-T04 归档后提交一致，暂存区为空。D10-T04 的 20 项签收源码及 26 项已登记的 Release 对象、库和产品逐字节匹配。正式 Release 构建完成，`Physara.exe --hidden --poll-limit=15` 退出 0。原生设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0 驱动 32.0.101.6129，有效 VSync 0。 [输入清单](../../../../build/d10-t05/a/input-manifest.json) · [正式构建](../../../../build/d10-t05/a/product-release.log) · [D10-T04 签收证据](../2026-11-26-switch-opaque-paths-atomically/tasks.md#final-verification)

四项获准的 D09 参考源文件字节与项目登记一致。历史 D09 临时夹具与原始图像已清理，本轮按参考参数以正式导入的铜球和 EXR、生成的见证板和冻结 SceneFrame 重建，分别登记输入身份与数值判据。D10-T01/T02 的宽索引、行序和预算合同作为前置；D10-T03/T04 证明原有角色准入及切换边界。本轮对双路径的全图、实际候选和本地性能取新证据。 [参考及重建参数](../../../../Docs/Performance/ForwardShadowReference.md#fixture) · [资产许可](../../../../Assets/README.md#forward-参考场景) · [临时夹具](../../../../build/d10-t05/a/Scenes.inc)

### A 原生接受与读回

`Replay-Release.exe reference` 通过真实编辑器采样、Renderer 图声明与原生提交接受 Forward 票据 24949 和 Forward+ 票据 24959，两路均读回工作色版本 2、主深度版本 1，RGBA16F staging row pitch 为 5120 B。Event 确认读取前提交已经完成。当前设备的公共 staging 支持 RGBA16F，D32 在同一已接受的只读图出口完成后使用原生读取。撤去视口采样时，新请求 Forward+ 与旧有效 Forward 并存，回放拒绝为该轮取得新样本。 [回放日志](../../../../build/d10-t05/a/reference.log) · [Forward 票据](../../../../build/d10-t05/a/reference-forward.json) · [Forward+ 票据](../../../../build/d10-t05/a/reference-plus.json) · [临时观察点](../../../../build/d10-t05/a/runtime-observation.diff)

### A 局部灯及相机边界

V2 在正交、`near=0`、有效矩形 `(0,20,640,440)` 的同一场景比较实发的全灯、常规 Forward+ 和受限网格 Forward+。混合预算为 153 条，102 格溢出。原生分支观测在点光的正常格读取 1 条有限灯与 2 条全局灯，在只有全局灯的正常空格遍历 2 条，在 spot 的 overflow 和 10 m 处的无有效格分别遍历全部 4 条局部灯。6 组原生全图差分的 HDR、D32 与覆盖误差均为零。锥边与径向标样通过。删除红灯后指定受光点预曝光 R 降低 0.45454，覆盖与范围外红色值保持匹配。 [原生分支](../../../../build/d10-t05/a/local-map.log) · [标样明细](../../../../build/d10-t05/a/local-camera-witness.json) · [原始差分](../../../../build/d10-t05/a/local-camera-difference.json)

V3 在无限远透视和 `(56,0,528,480)` 留边内读取近界 0.126 m 与有限分箱远界 12 m 的标样。原生深度与 `near / depth` 的差分别约为 `2.93e-8` 和 `3.10e-10`。近界片元选择正常格，12 m 处选择完整灯退路，6 m 受控有限网格也保留该片元贡献。共同平移 −33,554,432 m 后，两条路径的 5,984 个覆盖像素和全图颜色仍匹配，原位置的覆盖为 21,950 像素。空灯组保留收光面和真实 Forward+ 输出，实际有效候选为零。 [无限远透视](../../../../build/d10-t05/a/camera.log) · [原生查询](../../../../build/d10-t05/a/camera-map.log) · [远移](../../../../build/d10-t05/a/translated.log)

### A D09 阴影及环境对照

V1 在两路实际接受的 PCF 512 上使用同一冻结输入。每路消费方向光、聚光及点光共 3 组、11 层，实际 IBL 及天空开启。640 × 480 全图的 RGBA16F 四通道最大绝对误差、RMSE 与差异像素数均为零，D32 和覆盖相同。重建见证板上有 274,344 个地板深度样点、280,896 个覆盖像素。D09 原场景和原始数据已清理，历史参考记录的 274,168 个地板样点作为来源说明，本轮判据对当前重建实测。画外投射、镜像聚光、点光遮挡与接缝、MASK 实体与孔的标样均符合 D09 预期。保持直接灯不变时，去掉环境让受光标样的蓝通道下降 0.03688，真实正交天空恢复为无环境清色。 [全图差分](../../../../build/d10-t05/a/reference-difference.json) · [独立标样](../../../../build/d10-t05/a/reference-witness.json) · [环境对照](../../../../build/d10-t05/a/reference-noenv.json)

### A 多灯画质、观测与计时

规模场景固定单样本 Physical Lighting、None、640 × 480、正交零近平面、相同曝光，关闭阴影和 IBL。可见局部灯数在实际 View 之后核对为 0、32、256。每档同一冻结输入上，Forward 对 Forward+ 的 HDR/D32 全图差异为零，256 灯强制溢出也等价。对照回放用相同生产表面位置和有效覆盖区分实际分支。常规 0/32/256 灯的正常格有效局部候选范围为 0、0–5、6–30，289,536 个受光像素的退路比例均为 0。强制溢出组为 900/7,200 格（12.5%）及 289,536/289,536 受光像素（100%），每次退路访问 256 行。校准中原接收面 5 m 恰在层共边界，零灯观测的受光像素均走保守退路。常规对照将接收面与灯同时沿 Z 移至相邻非边界位置，重新通过画质与分支核验。 [多灯全图](../../../../build/d10-t05/a/scale-difference.json) · [两种退路比例](../../../../build/d10-t05/a/fallback-ratios.json) · [深度共边界的原始观测](../../../../build/d10-t05/a/calibration/scale-map-0-plus.json)

分箱区间包围实际列表构造，Froxel 载荷准备区间沿现有 `FroxelUpload::Prepare`，上传录制时间按真实写回调取样。不透明颜色查询包围共享的实际绘制回调，计时期间独立图 Pass 各保有其范围。每个接受的 Forward+ 暖帧实际发出 4 笔 Froxel 写，合计 2,163,388 B，同时有 4 个新原生缓冲存储。单独的传输 GPU 区间未测，整图 GPU 时间另列。两路径整图均为 8 次 Draw；Forward 为 41 次绑定，Forward+ 为 45 次。零灯 Forward 票据 243 在复用场景输入时出现 3 次图纹理及 1 次图缓冲分配，保留原样本及操作量。 [实际边界及发行计数](../../../../build/d10-t05/a/runtime-observation.diff) · [详细分布与计数](../../../../build/d10-t05/a/timing-summary.json)

七组各三轮交错，每轮暖机后取 24 个接受帧，共 504 帧。整图与颜色区间均取得 504 条 Ready 结果，逐条匹配原票据及该帧实际路径、Scene/View、Shader 代与布局。采集全部保留 1,276 个原始终态。同轮 Poll 465 解析了控制票据 420 和 421 的整图结果，删除一条已就绪事件的对账负向控制被正确拒绝。样本和原始终态随同一次进程记录。 [取样日志](../../../../build/d10-t05/a/timing.log) · [逐票据验收](../../../../build/d10-t05/a/timing-summary.json) · [零灯原始查询](../../../../build/d10-t05/a/events-0.csv)

以下为 Release、Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 32.0.101.6129、有效 VSync 0 的实测时间。单位为 ms，列值为 median／p95。CPU 工作合计采用 UI、准备、构图、编译与执行区间，槽与交换等待的分布单独记录。

| 局部灯和路径 | CPU 工作 | CPU 分箱 | CPU 载荷准备 | CPU 上传录制 | GPU 颜色 | GPU 整图 |
|---|---:|---:|---:|---:|---:|---:|
| 0 · Forward | 0.777 / 1.529 | — | — | — | 0.287 / 0.312 | 0.596 / 0.683 |
| 0 · Forward+ | 1.982 / 2.580 | 0.010 / 0.022 | 0.666 / 1.002 | 0.286 / 0.545 | 0.969 / 1.003 | 4.490 / 5.976 |
| 32 · Forward | 0.937 / 1.570 | — | — | — | 7.653 / 7.911 | 8.801 / 9.290 |
| 32 · Forward+ | 2.336 / 3.225 | 0.211 / 0.381 | 0.643 / 1.017 | 0.302 / 0.756 | 1.951 / 2.022 | 5.638 / 7.307 |
| 256 · Forward | 1.180 / 2.523 | — | — | — | 55.200 / 55.215 | 56.472 / 58.069 |
| 256 · Forward+ | 4.953 / 6.131 | 2.460 / 3.133 | 0.719 / 0.943 | 0.371 / 0.703 | 8.249 / 8.511 | 12.956 / 15.808 |
| 256 · Forward+ 强制溢出 | 3.685 / 5.903 | 0.991 / 2.126 | 0.691 / 1.688 | 0.347 / 1.119 | 78.278 / 78.308 | 83.701 / 85.412 |

常规 32 灯的颜色区间相对各自零灯底线增加 7.366 / 0.982 ms（Forward / Forward+），256 灯增加 54.913 / 7.280 ms，强制溢出相对 Forward+ 零灯底线增加 77.309 ms。各轮颜色区间中位数保持相同的收益与退化方向。零灯仍产生固定块交付和颜色查询代价；有完整列表退路时，同一场景保有光照，同时丧失候选收缩收益。测量估计的是实际颜色区间内候选查询、访存与局部光照相关工作的合计增量。 [分轮分布及 GPU 增量](../../../../build/d10-t05/a/timing-summary.json)

### A 独立审查交接

A 已复核 V1–V4。真实设备上的 9 个回放模式由本轮同一 Release 可执行文件执行，原始结果、读取格式、观察补丁和构建依赖均登记。生产库及正式 Shader 包用于画质和计时，分支包仅用于带标记的诊断回放。本轮实测限于 Windows x64 Release 和登记的 Intel GPU／驱动，验收采用 None AA；Debug 与 FXAA 的已有覆盖见前置归档。现已交接问题单，待 B 独立复核并记录审查结论。 [交接清单](../../../../build/d10-t05/a/handoff.json) · [签收问题单](../../../../build/d10-t05/final/review-signed.md)

<a id="a-correction-1"></a>
### A 首轮整改

保留 B 首审的两处产品注释和原始问题单。整改后的回放、观测及判据均在 `build/d10-t05/a-correction-1/`，52 个 Release 输入和前轮 125 个登记的原生证据文件已复核身份。受影响的 8 个原生模式均退出 0。Draw、HDR 与计时继续使用正式程序，带标记的诊断包独立重链。 [整改清单](../../../../build/d10-t05/a-correction-1/handoff.json) · [首审原件](../../../../build/d10-t05/a-correction-1/received-review.md) · [输入](../../../../build/d10-t05/a-correction-1/input-manifest.json)

| CR | 本轮复验及结果 |
|---|---|
| CR-001 | 解析深度和 alpha 先于 HDR 检查，拒绝原回放的远移近端清色。原远板保留等价。FP32 对齐的新远移近端及远端均实际受光，27,440 个覆盖像素及全图 HDR/D32 匹配。 [深度与覆盖](../../../../build/d10-t05/a-correction-1/local-camera-witness.json) · [原生日志](../../../../build/d10-t05/a-correction-1/translated-covered.log) |
| CR-002 | 实际紧凑或全灯入口提供标签，共享逐灯求值提供访问次数。格 4304 的两组载荷摘要相同，绕行控制由紧凑 3 行切到全灯 4 行。复测既有分支与空灯。256 灯常规组 289,536 个受光像素均走紧凑列表，其强制溢出组全部走 256 行全灯循环。 [对照差异](../../../../build/d10-t05/a-correction-1/branch-bypass.diff) · [标样](../../../../build/d10-t05/a-correction-1/local-camera-witness.json) · [退路样本](../../../../build/d10-t05/a-correction-1/fallback-ratios.json) |
| CR-003 | 按参考恢复三面角尺度与 ROI，274,168 个地板深度样点吻合。有效 PCF 512 的 3 组 11 层、环境与天空标样及双路径全图数值等价通过。 [参考标样](../../../../build/d10-t05/a-correction-1/reference-witness.json) · [原生差分](../../../../build/d10-t05/a-correction-1/reference-difference.json) |
| CR-004 | 独立轴内标样区分 1.2 m 有限 spot 的径向边界，扩大范围后外点恢复绿色贡献。仅将无 range spot 的强度置零后，其目标受光点 B 降低 0.001110，点光贡献和 D32 可核对。涵盖原锥边及红点光负例的 V2 全图差分通过。 [两项独立对照](../../../../build/d10-t05/a-correction-1/local-camera-witness.json) · [差分](../../../../build/d10-t05/a-correction-1/local-camera-difference.json) |
| CR-005 | 复用已核对身份的 504 个暖帧，按每帧的预算、格数、有效索引及溢出格数先匹配质量工作量，再逐票据解析。七组分布保持一致，256 灯常规／强制溢出颜色中位数为 8.249／78.278 ms。内存换标的 144 行在汇总前被拒绝。 [复算](../../../../build/d10-t05/a-correction-1/timing-summary.json) · [原采样](../../../../build/d10-t05/a/timing-256.csv) |

本轮实跑仍为 Windows x64 Release、Intel OpenGL 4.6.0 驱动 32.0.101.6129 及 None AA。新原生证据取自受影响的 V1–V3 和 V4 诊断回放，V4 的正式画质与时间样本对应首轮真实提交，按上述合同复用。五项 A 自检已登记，待 B 复核。 [签收问题单](../../../../build/d10-t05/final/review-signed.md)

<a id="b-recheck-1"></a>
### B 首次整改复核与签收

B 签收 `v2.0@2e94c06064d32f1cfcf2000e6e1b1c7020ac3155`、输入清单中两处头文件注释和 A 首轮整改交接。交接清单 SHA-256 为 `7753b0e9f20fee6122a1024f9e39751db72ea57689fef2656044dc8bfa4759a9`，输入清单为 `b99f0b60e80d6d0da48ba8a3a60c25e27f17e84a660d3addaa55b29ab0f476ee`。已核对实际产品差异、未跟踪 change、回放程序、正式与观测 Shader 包、复用的原始输入和证据。产品接口关于工作色与主深度、颜色 Pass 和每次退路灯行数的注释与实现一致。 [整改交接](../../../../build/d10-t05/a-correction-1/handoff.json) · [签收输入](../../../../build/d10-t05/a-correction-1/input-manifest.json)

| 问题 | B 独立复核结果 |
|---|---|
| CR-001 | 关闭。用同一 Release 程序重跑 `translated-covered`，新近、远标样的 D32 和受光通道均符合预期，两路全图一致。旧近板退化为背景的样本保留为负例。 |
| CR-002 | 关闭。原生重跑 `local-map` 和 `local-bypass`，同一正常格 4304 在相同网格、灯行及覆盖下，实际访问由 3 行变为 4 行。登记的空灯和各规模退路标记图也经重新解码。 |
| CR-003 | 关闭。从修正后的原图数得 274,168 个地板样点，重算全部 11 个 D09 阴影 ROI、天空和 IBL 标样，正式两路 HDR/D32 全图一致。 |
| CR-004 | 关闭。按实际覆盖独立核对有限 spot 的轴内距离窗和单独关闭无 range spot 的受光差分，外点恢复的绿色值为 0.034790，无 range spot 的蓝色增量为 0.001110。 |
| CR-005 | 关闭。504 个暖帧共 1,008 个计时区间逐票据匹配，七组分布可复算。逐条交换 256 灯两组标签，144 行全部在工作量准入处被拒绝。 |

上述原生复跑与 A 使用同一 Intel OpenGL 4.6.0 驱动 32.0.101.6129、Windows x64 Release、640 × 480、None AA、VSync Off。V4 时间取自已登记的原进程，本轮重新对账，未重新采时。B 审查通过，6.1–6.3 留给 A 在签收版本上完成。 [B 原生票据与独立复算](../../../../build/d10-t05/b-recheck1/review-results.json) · [逐项复核记录](../../../../build/d10-t05/final/review-signed.md)

<a id="archive-closeout"></a>
## 归档与阶段推进

D10-T05 已验收并归档，D10 阶段出口通过。B 签收记录按原字节留存，记录内的相对链接以 Docs 为起点。D11-T01 的前置条件已满足。 [归档审计](../../../../build/d10-t05/final/archive-audit.json) · [签收问题单](../../../../build/d10-t05/final/review-signed.md) · [当前阶段](../../../../Docs/Developments/Developments.md#current-stage) · [下一工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t01)
