# Design

## Context

规划基准为 `v2.0` 的 `2e94c06`（D10-T04）。两条路径共用表面、深度、阴影与环境照明，Forward+ 在局部灯查询中按本 View 的 Froxel 选择候选。当前诊断给出 CPU 计划中的格数、索引量和单次退路遍历量。Runtime 发布正常接受场景的整图统计，现有 GPU 区间覆盖正常图，`latestGpu` 只保留最近一笔已解析值。 [Froxel 准备](../../../../Docs/Architecture.md#light-preparation) · [受光入口](../../../../Docs/Architecture.md#opaque-shading) · [统计](../../../../Docs/Architecture.md#diagnostics) · [查询实现](../../../../Runtime/FrameStatistics.cpp)

## Goals and scope

按包完成可复现的画质、候选完整性和成本验收。实测通过产品的 Scene、View、Shader 准备、图执行和提交链取得。验收专用的夹具、录制观察点与采集入口归 `build/d10-t05/`，复用现有 Runtime 宿主和 RHI 接口。渲染算法、Shader ABI、路径切换事务和创作合同沿用已采纳规格。Deferred 与后续 profile 由各自工作包负责。 [开发约定](../../../../AGENTS.md#workflow) · [架构依据](../../../../Docs/Architecture.md#tests)

## Decisions

<a id="frozen-inputs"></a>
### 1. 先匹配实际输入和被接受的输出

每对输出共用冻结后的 SceneFrame 内容与同一份资产、物件、材质和灯表。优先在原生回放中复用同一次 SceneFrame 准备，单 View 交替路径时锁定同一场景并核对跨槽快照的内容、灯行和可见工作集。登记构建配置、源码及本地差异、场景与依赖哈希、Shader 包与加载代、实际命令、设备与驱动。复用前置输入和覆盖时核对同一目标、身份、执行条件与判据。 [快照合同](../../../../Docs/Architecture.md#prepare) · [D10-T03 验收](../2026-11-26-add-forward-plus-opaque-path/tasks.md#final-verification) · [D10-T04 验收](../2026-11-26-switch-opaque-paths-atomically/tasks.md#final-verification)

为每路记录 Scene、View、内容版本、当前相机及 View 版本、布局、图代和正常接受票据。检查实际颜色 Pass 的策略及 Shader 角色、生产相机与稳定投影、曝光、阴影有效过滤及灯组、IBL 有效代、深度预通道配置、渲染与显示尺寸及有效矩形、预览、AA 和采样数。所有影响画质的输入都须在配对内一致。旧有效颜色可继续显示，验收只接纳本轮所请求路径真正接受的匹配输出。 [计划就绪](../../../../Docs/Architecture.md#plan-readiness) · [提交发布](../../../../Docs/Architecture.md#history-commit)

<a id="hdr-readback"></a>
### 2. 在工作色生产者处度量线性结果

在同图对实际 `ForwardOutput.workingColor`（天空合成后、显示映射前）和主 D32 声明读回，复用合法的 Copy、提交、Event/完成轮询、可用后 Map 和真实 row pitch。绑定每项读回的资源版本、次序及接受票据。按源纹理方向还原有效画布，保存预曝光 RGBA16F 编码及其解码值，比较全画布和受光标样，报告各通道最大绝对误差、RMSE、差异样本数及覆盖／D32 差异。若报告去预曝光线性值，使用实际生产帧的共同倍率。 [工作色生产者](../../../../Engine/Renderer/Paths/Forward.hpp) · [读回合同](../../../../Docs/Architecture.md#readbacks)

同构建、同设备、同画质的基线容差为预曝光 RGBA16F 和 D32 解码数值一致（`+0`、`-0` 数值等价），有定义的像素必须为有限数。当前两路径共用格式及求值，候选按完整灯行号顺序累加，容差以该合同为前提。出现差异时先定位覆盖、量化、灯行、贡献及生产身份。若发现真实且已说明的编码或算术误差，须先登记来源、可推导上界和独立失效对照，更新验收判据后再判定。 [光照合同](../../../../Docs/Architecture.md#lighting) · [累加顺序的原生回归](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification)

<a id="candidate-observation"></a>
### 3. 用原生观测确认候选与退路

同一标样需要可定位到独立预期的亮度／颜色变化，以及当前产品 Shader 的实际正常格或退路。正常格使用真实网格和本轮上传的 header、索引及全局灯，退路区分 overflow 和无有效 cell。可用一个不计时的原生观测回放，从相同受光位置、遮罩和深度覆盖取得共享 Shader 查询选择及有效候选数。必要的临时 Shader 观测沿用共享查询及实际图输入，单独登记包和差异。其匹配的正式包仍是 HDR 和计时的来源。 [查询、求值和退路](../../../../Shaders/Lighting/FroxelLighting.glsl) · [共享列表](../../../specs/froxel-light-preparation/spec.md)

分别记录 `溢出格数 / 有效网格总格数`、已观察的 `退路受光像素数 / 有效受光像素数` 和每次退路的完整可见局部灯行数。像素比例取匹配画质与工作量的确定性标样，列明覆盖口径，区分 overflow 与无有效 cell。用单样本、无重叠的受光工作集量度基准比例，空分母记为不适用。任一观测只表达自己的范围，候选数也保有灯行单位。

<a id="acceptance-spec"></a>
## 验收规格

| 情景 | 固定输入与操作 | 通过判据 |
|---|---|---|
| V1 · 阴影与 IBL 同口径 | 重放 [D09 参考](../../../../Docs/Performance/ForwardShadowReference.md#fixture) 的 Physical Lighting、None、640 × 480、有效 PCF 512 与 IBL。沿用登记的源资产及许可，按参考重建必要夹具并核对内容身份。分别取得正式 Forward 和 Forward+ 的接受输出。 | 配对身份和实际阴影组、层及过滤、有效环境和预曝光匹配。原生全画布 RGBA16F、D32 满足数值门槛，无遮挡／遮挡、MASK、天空及环境标样满足独立参考预期。已有符合身份的 FXAA 证据可复用。 |
| V2 · 全局灯、正常格与混合退路 | 同一场景使用有范围点光、斜向有限 spot、无范围点光与 spot，并保留方向灯前缀。在同一网格内安排正常有候选、只有全局灯的空正常格、overflow 和能合法出图的网格外标样。用现有 Froxel 请求约束索引额度，保留有效 header 和实际 Forward+ 颜色消费。 | 两路径的 HDR 和覆盖满足一致性门槛，正常格可证实消费紧凑候选。无范围灯在其有效照明位置保留贡献且一行一次，spot 外锥内外及有限范围内外有明确差别，overflow 和无有效 cell 样点使用完整局部灯表。抑制一盏在指定 ROI 有可辨认贡献的灯后，参考检查须能检测差异。 |
| V3 · 相机边界与空灯 | 在少量可定位的标样上覆盖无限渲染远面的透视（有限稳定分箱、近平面附近与超出分箱远距的有效受光片元），以及 `near=0` 的正交；至少一组采用有边距的有效矩形。对空场景灯表保留可绘表面。标明可复用的远移和精度覆盖。 | 受光标样在应有的 cell 或全灯退路上满足相同 HDR 和深度判据，边距与空集合结果可辨。正式 Forward+ 的空集合有效迭代数为零。大坐标复用以相机、表面、灯及相关包身份确认。 |
| V4 · 多灯成本和归因 | 跑 [测量协议](#benchmark-protocol) 的匹配组。通过同一产品图取得两策略的原生输出，设置一个受限索引额度的密集组作为退路对照。 | 每一比较组先通过质量与工作量核对。分箱、实际上传、GPU 不透明颜色及局部灯增量各有事实来源和单位；各区间归原接受输入及票据；比例与延迟样本可复算。将净变化、各阶段代价、退路和测量波动写入结论。 |

V2、V3 可共享场景及夹具。只在正样本无法区分错误结果时补对应对照。CPU 准备或单格内核旧证据可证明局部风险，阶段组合仍须有当前正式受光和接受输出的证据。 [D10-T01 验收](../2026-11-25-prepare-cpu-froxel-light-lists/tasks.md#final-verification) · [D10-T02 验收](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification)

<a id="benchmark-protocol"></a>
## 测量协议

1. 使用同一 Release 构建、设备及驱动、宿主、单 View、有效 VSync、分辨率、AA、几何覆盖和材质。成本工作集固定单样本 Physical Lighting、None、640 × 480、相机与曝光，阴影及 IBL 在该工作集均关闭以隔离局部灯成本。使用同一可重建光分布序列的 `0 / 32 / 256` 个可见局部灯，分别对应零灯底线、稀疏和密集规模。核验实际裁剪后的行数、位置及贡献；每一档的两路径使用其同一冻结输入。再在密集档的同一输入增加一个有效 overflow 对照。V1 保留有效阴影和 IBL 的实际画质基准。 [测量合同](../../../../Docs/Architecture.md#benchmark)
2. CPU 分箱只计实际进入 `BuildFroxelLightLists` 的网格、相交和压入范围。记录实际请求、使用状态、相关 View 和灯数。提交时把接受路径所用的准备与候选附带的额外准备区分开。Froxel 上传分别记录 `FroxelUpload::Prepare` 范围的 CPU 载荷准备时间、由实际存活写 Pass 发出的原生笔数与字节及其 CPU 录制时间。按所属 View 和四个块追到实际写入，核对空列表的已定义绑定和固定块范围，共享上传按事实计一次。若可以单独取得 GPU 传输区间，独立标明测量边界。 [分箱调用](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [Froxel 上传](../../../../Engine/Renderer/Lighting/FroxelGpu.cpp) · [图上传合同](../../../../Docs/Architecture.md#uploads)
3. GPU 使用现有 RHI 时间戳在实际不透明颜色 Draw 的同次录制包围区间，按所属 View 的 `colorPass` 定位。其范围含该 Pass 的真实绘制及片元工作，前置深度、独立阴影生产、上传、天空、显示和 UI 各有自己的范围。在同路径、同一批次和覆盖上，用相同固定条件的零灯区间作基线，报告高灯数相对零灯的增量与分布，并用同灯数强制退路比较候选变化。局部灯成本以该原生颜色区间增量估计，包含相应的候选查询、访存和着色影响。整图 GPU、CPU 工作、FrameSlot 和 PresentWait 各自保留原单位和范围。 [图执行](../../../../Engine/Renderer/RenderGraph/GraphExecutor.hpp) · [原生完成查询](../../../specs/opengl-completion-queries/spec.md)
4. 每组使用三轮交错的匹配窗口，先让各帧槽、Shader、驻留和图池达到稳定复用，再以每轮 24 个接受暖帧作为起始取样窗口。冷帧、过渡帧、投递或 Present 失败按实际状态另记。测试双方的观测、UI、预热条件一致，图像读回、退路诊断及详细追踪在匹配输入的独立回放中完成。按档报告接受次数、GPU 有效样本量和缺测原因，以及 CPU 与 GPU 区间 median／p95、分轮差值、上传、Draw、绑定和已观测的分配数量。只补输入变化、失败或缺失影响判定的组。
5. 在查询被汇总到面板之前消费同一次非阻塞 Poll 取得的每个已解析区间。每行用设备提交票据、View、Scene 及版本、实际路径、Shader 代、布局和区间范围对应当次采样。已解析结果与当次原始事件逐行核对，同轮多条返回亦保留。Pending 保持待决，后续非阻塞收尾写出 Ready、Skipped、Unavailable、Failed 或有原因的未完成记录，缺测与合法 0 区分。锁定身份和测量口径后对已匹配样本复算分布；真实缺失使一个比较组无法完成时，该组保持待验证。 [异步计时合同](../../../specs/render-frame-statistics/spec.md) · [既有原生查询对账](../2026-11-25-verify-shadow-regressions-and-resource-pressure/tasks.md#final-verification)

## Risks and trade-offs

- 两幅图即使相同也可能贯穿完整灯退路。接受 V2 需要实际正常格紧凑候选、退路及移除已知贡献的证据，V4 同时报告两种比例及真实颜色 Pass 的工作量。
- 布局、曝光、准备代或阴影消费有细小差异时，孤立的 ROI 能掩盖全图误差。先核对真实生效的生产身份，以当前 Forward 作为同时期对照，再用 D09 参考证明固定输入保有原预期。
- GPU 结果可能在同一轮返回多条，固定数量也可能遇到池压力或设备时钟波动。独立记录每项终态和原票据，按分轮分布限定结论。未分辨稳定收益的有效样本可记录为实测结论，阶段出口依赖匹配画质、可解释的分项数据及完整性证据。

## Delivery

在与 A/B 签收一致的输入上完成最终 verify。原始图像、CSV、事件及可重放命令归工作包临时目录，必要的输入与生成参数、版本、判据、实测数据和复现限制留在 tasks。最终形成供 D11-T05 使用的 `Docs/Performance/ForwardPlusReference.md`，引用 D09 基准及本 change 的原生结果，填入已签收数据后随验收提交。 [后继基准](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05) · [文档归属](../../../../Docs/Architecture.md#project-docs)
