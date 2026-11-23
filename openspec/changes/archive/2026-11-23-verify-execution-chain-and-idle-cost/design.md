# Design

## Context

[D08-T08 与阶段出口](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t08) 要求在同一交付版本验证准备、执行和退休的交界。规划时 `v2.0` 的基准提交为 `cc0345a`（D08-T06），D08-T07 的交付源码、主规格与归档在工作区。实施时以实际构建输入重建身份，纳入相关未跟踪源码，基准提交仅用于定位差异。

[统计快照](../../../../Docs/Architecture.md#diagnostics)规定各层事实的归属，[合同验证](../../../../Docs/Architecture.md#tests)要求原生同步以真实结果和完成证明判定，[测量合同](../../../../Docs/Architecture.md#benchmark)规定同画质及采样口径。排序和图执行边界可对照锁定的 [Filament 绘制命令](../../../../Docs/Reference/Filament/VisibilityAndCommands.md#f6)；提交、上传版本与资源回收的区分可对照锁定的 [NVRHI 完成与回收](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n10)。原生判定使用 Physara 当前的 OpenGL 合同。

## Goals and scope

阶段出口覆盖实际接受的静态 Forward 场景、一个主 View 加一路附加离屏 View，以及当前单队列上的原生执行。用现有能力规格给出可重复的输入、观测点和判据。以同一版本、同一 profile 的分组实验说明优化效果，给出实测设备支持范围和后续重评项。

## Decisions

### 1. 选择一份可共享的固定输入

- 从获准用于 Physara 的 [Forward 参考场景](../../../../Assets/README.md#forward-参考场景)派生固定回放，在临时资源树中布置可辨认标样、同源重复实例和不同几何范围。两路相机的可见集合有交叠也有独占项。批处理对照中有非零 baseVertex、firstInstance 和至少两个兼容间接记录，位置避开会因顺序改变的等深重叠。按实际打开的文件记录场景、模型及依赖的 SHA-256、来源许可和派生参数。
- 启动使用正式包和真实窗口/GL host。帧回放沿 [Runtime::RenderFrame](../../../../Runtime/Frame.hpp) 的 `OffscreenViewRequest`、正式 EditorSession 输入和 Renderer/Graph/RHI 路径驱动，同一接受提交分别观测主视口及离屏输出。必要的调度与观测写在 `build/d08-t08/`，产品启动沿[现有入口](../../../../Docs/Build.md#运行)。`--poll-limit` 只限制事件轮次，测量按实际接受的场景帧计数。
- 优先用已归档的局部证明和可取得的前置回放。复用结果时核对相关源码、编译选项和配置、Shader 包、实际库和可执行文件、fixture、设备及驱动身份。前置归档提供原合同与检查入口，本轮组合场景须有对应当前输入的证据。[增量与多 View](../2026-11-19-prepare-incremental-scene-views/tasks.md#final-verification) · [版本化驻留](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification) · [批处理](../2026-11-20-complete-draw-batching/tasks.md#final-verification) · [Compute](../2026-11-21-enable-opengl-compute-synchronization/tasks.md#final-verification) · [传输与 queries](../2026-11-22-complete-opengl-transfer-queries/tasks.md#final-verification) · [图资源池](../2026-11-23-complete-render-graph-resource-pooling/tasks.md#final-verification) · [Shader 热重载](../2026-11-23-complete-shader-variant-hot-reload/tasks.md#final-verification)

### 2. 按事实归属对账并关联原生证据

| 事实来源 | 判定口径 |
|---|---|
| Renderer / FrameSlot | 同一槽安全取得时一次共享提取，各 View 单独裁剪、请求与输出。提取更新与静态编码分别计量。Forward 在存活 Pass 中的静态、每 View、实例及间接参数上传分别归类。 |
| Renderer / Draw 与 RHI | 所选 View 的可绘表面计一次，批次和间接记录按执行的 Depth/Color Pass 计。整图 RHI 计数包括实际参与的 UI；一次 MDI 是一次原生 Draw，绑定按实际状态变更，上传按实际写调用与紧密字节。以帧身份和采样区间匹配实际命令，账面需求单独读取。 |
| RenderGraph / 资源池 | 候选图和有效接受图各有身份。核对声明、存活、裁剪及图计划逻辑容量，和池的实际兼容命中、物理对象、真实 native 创建、淘汰及压力。逻辑预算按物理对象去重，录制、在途、外部引用与空闲按 owner 的状态及重叠口径解读。 |
| Runtime / Editor | [统计 owner](../../../../Runtime/FrameStatistics.hpp)分别保留最后正常场景、最近图、当前池及 Shader 状态。编辑器读取[快照](../../../../Editor/Session/FrameStatistics.hpp)，自身 UI 构建与提交独立计量。GPU Ready 带原场景提交和计时区间，PresentWait 来自窗口交换。 |

在同一个观测序列中区分 CPU Ready、图接受、原生交换、Event/Fence 完成和缓存淘汰。时间和容量计数以各自 owner 的实际采样范围对账。用独立的阶段读回标样核对数据与颜色；性能采样和 Pending 观察期间保持正常命令流，等到合同允许的真实完成后读取。必要时暂缓一次本地完成或可用性观察，注明受控位置，随后从原生查询取得实际完成。

<a id="verification-matrix"></a>
## 验收规格

| 检查 | 场景与通过条件 | 产品规格 |
|---|---|---|
| V1 · 冻结与批次对账 | 每个槽完成首次接受和安全复用后，固定非空场景继续渲染。已暖槽的 SceneFrame 重建、记录改写、静态编码和已驻留静态上传保持零，兼容稳定需求复用池与程序对象，容量及 arena 增长可解释。每路 View 的动态常量、实例/间接参数和实际 UI 工作按来源列出。用一帧代表性原生命令核对深度、颜色、实例与间接记录、原生 Draw、绑定、上传和创建。 | [SceneFrame](../../../specs/scene-frame-preparation/spec.md) · [静态驻留](../../../specs/static-gpu-residency/spec.md) · [Forward 批处理](../../../specs/static-forward-rendering/spec.md) · [原生 Draw](../../../specs/opengl-immediate-draw/spec.md) · [帧统计](../../../specs/render-frame-statistics/spec.md) |
| V2 · 稀疏编辑与独立 View | 在 V1 暖机后只提交一个可采样源槽的材质因子编辑，再单独改变离屏 View 的相机或尺寸请求。各安全槽追赶受影响记录和资源版本，未相关的编码及上传保持复用。两路共用一次提取，各自的可见集、有效设置、像素及接受身份符合预期。只改 View 的轮次保持静态内容身份。 | [SceneFrame](../../../specs/scene-frame-preparation/spec.md) · [View 准备](../../../specs/view-preparation/spec.md) · [编辑器帧图](../../../specs/editor-frame-graph/spec.md) |
| V3 · 连续加载与池压力 | 在真实槽轮转中切换有可辨输出的文档或资产版本，并让离屏尺寸往返一次。对照请求、固定版本、接受的图像和池的容量/命中/创建。旧 View 或录制仍持有的对象保留到相应引用解除与真实完成，只有兼容且已可回收的对象参加重用。使用相符的前置证据或一次代表性的准入压力，核对上限、当前原因和最近正常场景身份。 | [CPU 资产发布](../../../specs/cpu-asset-publication/spec.md) · [静态驻留](../../../specs/static-gpu-residency/spec.md) · [图池](../../../specs/render-graph-resource-pool/spec.md) · [帧槽寿命](../../../specs/frame-slot-lifecycle/spec.md) |
| V4 · 真实跨消费域执行 | 用现有图/RHI 原生回放令 Compute 写入有已知标样的 image 和 indirect Buffer，随后 Draw 采样并索引间接绘制，再对同版源做 transfer 与 staging 读回。消费前域依赖按实际资源和范围出现；命令在正常顺序中发出一次。通过匹配提交的 Event 观察真实完成后核对绘制像素、源标样、参数字节与行间距。以同一回放检查未完成读回的拒绝，待测命令间保持 GPU 依赖并避免全域 barrier 和设备等待。 | [Compute 同步](../../../specs/opengl-immediate-compute/spec.md) · [原生 Draw](../../../specs/opengl-immediate-draw/spec.md) · [传输和 staging](../../../specs/opengl-immediate-transfer/spec.md) · [EventQuery](../../../specs/opengl-completion-queries/spec.md) |
| V5 · 统计显隐与 Pending | 对齐同一输入及其接受图，显示和隐藏 Frame Statistics。场景/图/池/Shader 的事实身份和归属一致，UI 自身实际差异在整图计量中可解释。至少观测一份实际时间戳区间先 Pending、再按其原票据 Ready；未就绪时只有可用性观察，面板与正常轮询都不取阻塞结果，真实 Ready 后秒数转换正确。跳过或不可用保持其状态。 | [帧统计](../../../specs/render-frame-statistics/spec.md) · [非阻塞 queries](../../../specs/opengl-completion-queries/spec.md) |
| V6 · 在途热重载 | 在两路实际消费的 Shader include 上触发有位置的失败，再保存同 ABI 且有可辨输出的修复。原生准备按真实额度推进，Pending/Failed 保留旧完整代；修复后两路冻结计划引用同一就绪代并由各自的接受票据确认画面。新代提交时追踪至少一份旧代在途依赖，有真实完成且引用解除后才允许退休。核对源请求、实际 CPU 编译、原生创建与 View 统计各自的身份。 | [Shader 代](../../../specs/shader-generation-reload/spec.md) · [编辑器帧图](../../../specs/editor-frame-graph/spec.md) · [帧统计](../../../specs/render-frame-statistics/spec.md) |

V1、V2、V3 和 V6 可共享固定场景回放。V4 使用实际支持的 Graph/RHI consumer，保持生产 Forward 路径的职责。一个覆盖真实版本与完成关系的代表用例即可，局部格式、ABI、参数上界和错误矩阵沿用身份匹配的前置证明。

<a id="benchmark-protocol"></a>
## 同画质性能对照

1. 取当前验收范围的同一源码和同一 Release 构建参数，以生产执行为基线。增量对照在临时构建的既有准备边界强制全量提取和静态 CPU 重新编码，保留相同版本协议、驻留策略与批处理。批处理对照在另一临时构建使用现有直接索引 Draw，保留表面排序、索引范围、baseVertex、firstInstance 和材质，按表面拆开实例/间接批次。直接对照沿同一帧图，仅声明和上传其实际使用的数据，保持增量准备。对照从同一来源派生，每次只改变一项被测策略，差异及实际产物写入清单。
2. 固定静态场景及相机、曝光、材质/扩展比例、灯和环境、Forward 模式、实际渲染与显示范围、AA、View 数与 UI 布局、有效 VSync 及调试观测配置。在真实设备上以固定单样本 Physical Lighting 作为初始测量 profile。比对匹配接受输出的预曝光 HDR、Reverse-Z 深度及显示线性颜色，验证可见表面、设置和环境相同。冻结段的画质输入取首个编辑前的加载态，关联实际计时的场景文件及接受内容版本。稀疏段沿同一源槽的覆盖颜色验证编辑前后结果。登记采样位置、色域、数值容差及误差统计，保留能发现错材质、错实例和错误深度的标样。
3. 三种执行先让各槽、驻留、池和程序完成暖机。增量比较冻结段及固定稀疏编辑序列；批处理用重复实例和兼容命令的冻结段。各用同一段匹配接受帧，待测方案与对照交错执行。起步使用三组成对轮次，每轮 120 个已接受帧；按观测到的方差说明采样充分性，只有结果受噪声影响且补采有判别价值时才追加。稳态窗口内关闭额外原生跟踪和像素读回，统一使用已有统计及非阻塞计时。若对照未通过等画质或采样条件，保留失败和原因并补正确对照。
4. 报告 CPU 的准备、图声明、图编译、图执行与 UI 时间，GPU 的同范围有效 Ready 样本，单独报告槽/事件等待和实际 PresentWait。临时测量在各执行侧从 UI 准备完成后登记图用量起，到图封存为止取样图声明，使两侧的参数准备都进入同一组 CPU 区间。对每组列出样本数量、median/p95、轮次离散程度及 Pending、跳过、失败和缺失数。列出 CPU 改写、静态/动态上传次数与字节、原生分配、绑定、Draw、批次和池的用量变化。GPU 结果按原提交配对，缺失维持缺失。保留接受与实际交换结果，说明图内 UI 是否参与。
5. 对增量和批处理分别给出收益、退化或证据不足以区分的结论，说明质量门槛、波动与适用范围。未证实收益的复杂优化把复现入口、成本和待重评假设交回 [D08-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01) 或 [D08-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t03) 的能力范围，按对应 OpenSpec 处理后续变更。两项比较均须有有效对照证据；保留分布覆盖不足或设备缺失所造成的待验收项。

## Risks / Trade-offs

- 全量对照的计时维持正式驻留与上传策略。画质门槛让重新编码的对象行、材质行和灯表决定参考画面，取同一覆盖槽的前后两种颜色。定向保留旧材质行的对照应被编辑后 HDR 和显示像素比较拒绝。画质运行与计时窗口独立。
- 同步读回可掩盖资源过早重用。Pending、真实完成及读回依次取证，等画质确认在计时窗口外完成。受控延迟仅用于稳定观测顺序，记录与自然时序的差异。
- `latestScene`、`latestGraph`、池和最近 Ready Timer 刷新时点不同。以各自的 View、图代、资源代和提交身份配对，避免将旧值放入新帧或把最新候选的计数加到上一有效帧。
- 不同历史提交改变了计数和执行路径。以同一当前版本派生单因素对照，记录临时差异、构建和合格样本；若调度或有效 VSync 不同，重做受影响的配对。

## Migration Plan

1. 先固定验收输入和可复用证据，在工作包临时目录完成回放、质量比较及取样。已采纳合同的实现偏差保留复现，记录最小修正和定向回归。
2. A 在现行问题单交接，B 独立核对证据与涉及的源码/注释，A 整改，B 复核。B 签收后 A 按最终输入做原生 verify 并保留结果、支持 profile 和限制。[独立审查闭环](../../../../.agents/skills/physara-review/SKILL.md)
3. 按 `.openspec.yaml` 的最终规格范围处理同步，归档后更新工作包、D08 出口和 D09 前置，留存审查签收后重置问题单。需要重现的参数、命令和数据摘要写入 tasks，临时文件可随 build 清理。
