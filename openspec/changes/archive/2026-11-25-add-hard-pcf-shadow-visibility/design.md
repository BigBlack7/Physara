# Design

## Context

当前 `ShadowPlan` 按 View 交付成组区域，稳定逻辑地址可以离散。Forward 的一次诊断将选定组压入一个 D32 数组，以单层二维视图取样。普通光照用 `PrepareLightTable` 的完整灯表，`EvaluateLighting` 与 `EvaluateDirect` 已共用预曝光线性色域。图支持保留只读层组，图形包和 OpenGL 采样视图还需表达实际的二维数组绑定。 [提案](proposal.md) · [灯光合同](../../../../Docs/Architecture.md#lighting) · [现有层组交接](../../../specs/render-graph-execution/spec.md#requirement-complete-declared-resource-handoffs-at-the-end-of-execution)

## Goals / Non-Goals

- 在 View 准备阶段确定实际消费的阴影输入，让普通光照和诊断共享查询、坐标、偏移及过滤。Renderer/Lighting 保有计划和灯对应关系，Shaders/Shadow 保有查询，Forward 负责资源组装。
- 当前交付路径为静态 OpenGL Forward。Forward+、Deferred、透射分支、PCSS、moments 和 D09-T05 的压力门槛各沿自己的工作包推进。 [路径合同](../../../../Docs/Architecture.md#opaque-shading) · [D09-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05)

## Decisions

### 会话设置与可消费范围

- 扩展 View 的类型化阴影请求。保留默认关闭、50 m、1024 texel 和已定义级联设置，新增默认 Hard 与 PCF 选择，以及 4、8、16、32 MiB 的逻辑预算档，默认 32 MiB。主 View 仍通过 Renderer 面板和 EditorSession 提交，离屏 View 走同一边界校验。非法请求整体保留上一个有效值。过滤属于查询输入，预算和边长传给实际规划。 [View 增量](specs/view-preparation/spec.md)
- 有效额度取用户请求、项目上限与当次可用能力的交集。沿用 16 层硬上限及主方向、聚光、点光顺序，同类依稳定灯身份排序。保持 D09-T01 的共同边长策略，首个能准入的优先组选择合法的最高请求档，后续组在该布局成组准入。按计划层跨度乘以完整层字节核算，所以物理紧凑映射的空洞消除不会放宽准入规则。 [区域准入](../../../specs/shadow-planning/spec.md#requirement-allocate-bounded-shadow-regions-with-stable-light-correspondence)
- 准备器按当前实际显示配置选择阴影读范围。普通受光分支使用所有已准入组，合法诊断使用被选组，其他预览和空工作集没有 GPU 阴影需求。诊断从当前场景失效时，按普通分支重新决定所需组和附件，沿既有离屏预算与一次结算规则处理。设备不具备所需能力或规划明示拒绝时，以中性可见性和相应理由进入普通光照。真实准备、录制或提交失败沿原等待、拒绝与保留上一有效输出的路径处理。

### 一个比较核合同覆盖三类投影

- 保留常规 D32、`clear=1`、`Less` 写入以及接收深度 `<=` 保存深度的比较。复用 caster 几何、MASK、镜像、双面和写入 bias。取样用整数 texel 和 mip 0 的原始深度。Hard 对中心 texel 求一次比较，PCF 以该中心的整数偏移 `[-1,1]²` 做九次比较并计算均值。等权网格给出可复核的最大核和分数可见性。Poisson 是采样分布，留待需要不同分布的 profile。 [深度约定](../../../../Docs/Architecture.md#conventions) · [过滤合同](../../../../Docs/Architecture.md#shadows) · [Filament 参考](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f9)
- 约定物理边长为 `E`，每侧保留 `G=4` 个物理 texel，内区边长为 `E−2G`。CPU 与 Shader 共用由有效边长和拟合产生的尺度及边界。先预留一个 texel 的核半径，再留接收偏移最多半个 texel、级联中心吸附最多半个 texel、FP32 投影误差最多一个 texel 及边界取整余量。有效接收域上的投影误差及可取样范围用实际交付系数核对。只要相同布局成立，两档过滤共用这份生产覆盖。任何最终 footprint 的判断均先于 fetch。
- CSM 用每级完整 receiver 球半径 `R`，以 `R/(1−2G/E)` 作为物理光空间半宽，按 `2*halfExtent/E` 米每 texel 吸附。caster 候选 XY 扩到覆盖实际最远 tap，Z 拟合纳入画外遮挡和所需偏移/舍入余量。稳定线性视深和过渡带位置使用原接收点，依次用两个级别各自的偏移及比较核，再混合 visibility。 [规划增量](specs/shadow-planning/spec.md)
- 聚光和点光保持 D09-T03 的米制半径、轴向近远面和面顺序。扩展横向投影令 `tanExpanded = tanNominal * E/(E−2G)`，其中 spot 的名义值来自外锥半角，point 为 1。聚光按未偏移点在扩展投影中的名义 NDC 圆判定外锥，圆半径为 `(E−2G)/E`。点光继续对未偏移的灯相对向量取绝对值最大分量，平手按 X、Y、Z。每个 tap 留在同一扩展面内，整个核可跨越名义面边及角，caster 覆盖保守包含扩展面可达的遮挡。扩展仅服务取样和 caster 筛选，名义灯体相交、半径和直接光锥角按原值决定。透视比较量取该面的 `z/w`。保留窄锥实际打包系数的 XY/W 误差与近端写入精度检查。拦截不能证明余量的区域，保留精度拒绝原因和完整源灯。
- 接收偏移在世界米制的相对位置上加到接收点，使用几何法线朝向入射灯的一侧。偏移上界为半个世界 texel，并用 `1−abs(dot(Ng,L))` 在法向入射时降至零。正交使用本级固定世界 texel，透视用未偏移轴深 `t` 的 `2*t*tanExpanded/E`。对透视按分母变化进一步限幅，仅保证 XY 位移不超过半个物理 texel。例如对 `H=tanExpanded` 和 `abs(ndc.xy)<=1`，令偏移长度 `a` 满足 `a/t <= H/(E*(1+H)+H)`，可从 `abs(Δxy), abs(Δt) <= a` 得到上界。按比例稳定求值，并沿该方向限制到当前投影近远面内再比较。零可用偏移时仍按原点的有效投影比较。普通颜色和诊断都从现有相对位置表示及同一几何法线取得该偏移，主相机 Reverse-Z 和 normal map 保留各自用途。

### 匹配灯表与图上的实际层

- 将本轮所需组稳定地紧凑排列在一份同边长、单 mip、单样本的 D32 二维数组。顺序用准入组顺序和组内子视图顺序。CPU 从计划逻辑层、灯代及内容代明确得到物理层，生成与取样共用映射。一个有效组的物理层连续，spot 可得到仅一层的数组视图。此布局保留精确的逐层附件写入，同时使多灯只消耗一个采样图像位置。使用 atlas 会连带引入矩形清除及区域写入合同，也会改变现有层级预算口径。 [深度生产与消费](specs/shadow-rendering/spec.md)
- 各层用对应投影和已有静态绘制生成，空 caster 层亦 `Clear/Store`。颜色及诊断声明实际消费的最终版本和 mip/layer/Depth 范围。编译器和执行器用现有子资源跟踪安排写入、着色读转换和图尾交接。录制前用解析出的同范围显式数组 SRV 绑定，单层图形附件仍为二维视图。未定义层、超出声明或设备能力的区间在各自边界拒绝。保持旧的单层采样和只读层组导出合同。 [图合同](specs/render-graph-execution/spec.md) · [NVRHI 子资源参考](../../../../Docs/Reference/NVRHI/ResourcesAndPipelines.md#n3)
- 权威生成接口增加普通浮点 `texture2DArray`，以单个 Texture SRV、整型层 fetch 进入 GraphicsBaselineV1。维度随 ABI、SPIR-V 反射、包和活动绑定核对，通过原有 texture-only fetch 的 OpenGL lowering。Native 只接受有真实能力的二维数组同格式视图，明确检查维度、非空合法的连续范围以及 D32 深度解释。格式能力按实际 texture target 确认。 [Shader 包](specs/shader-build/spec.md) · [OpenGL 绑定](specs/opengl-shader-pipelines/spec.md)
- 从冻结灯身份到已编码灯行建立阴影组引用，当前可用组从 1 开始，0 表示中性。用现有生成接口发布有限的组及投影/层表，上界来自 `MaxShadowLayers`。现有灯表缓存键比较有效行来源及实际引用，只有引用发生变化时才为映射原因重传灯行。表中的投影内容可独立更新。Shader 按已知灯行取对应引用，排序和不同 View 的引用各自匹配。 [灯表增量](specs/punctual-lighting/spec.md)
- `EvaluateDirect` 为当前反射直接光分支提供表面位置、几何法线、入射方向和组引用，乘一次 visibility。先沿用当前无贡献灯的提前结束条件。环境、发光、预曝光和表面响应由各自已有路径计算。诊断调用同一查询，将所查级联或面用于原诊断配色。沿现有变体/准备代加入带阴影绑定的受光变体，Off 或无可用组仍可用普通无阴影变体。Hard 或 PCF 为帧数据条件，避免为相同 ABI 增建独立程序。已接受的匹配程序代可以继续使用，失败候选沿现有热重载和出图合同处理。 [Forward 增量](specs/static-forward-rendering/spec.md) · [程序准备合同](../../../../Docs/Architecture.md#plan-readiness)

<a id="depth-reuse-and-statistics"></a>
### 深度复用与统计归属

- View 持有当次匹配且实际被消费的完整只读数组。复用键包括设备、View、Scene、实际消费布局、灯代、逻辑到物理层映射、区域及必要内容版本、写入 bias 和实际深度程序。整套匹配时通过 `ShaderRead` 保留版本显式只读导入，跳过 caster 准备及深度写。仅变更过滤、曝光、灯光度或实际 jitter 时，深度生产输入可继续匹配。关阴影、无消费或布局变化时结束失配保留对象的当前资格。
- Forward 声明深度生产和颜色采样依赖。组图方建立实际颜色消费者后才为该路的新数组声明完整的 `ShaderRead` 出口。下游自行声明独立深度读回所需的范围及依赖。
- 本次以整套消费布局作为 GPU 缓存替换单位。一个输入改动后，在匹配的新数组上生产当前需要的层。CPU 仍维护逐区域所需版本。整组替换使已接受旧数组及在途读保持不可变，避免首次接入即引入 D32 跨数组拷贝或可写 View 输出。代价是稀疏编辑、预算变更或诊断切换后可能重画未脏层。记录这类生产成本供 D09-T05 判断收益。每 View 的逻辑规划上限仍为 32 MiB，精确计算当轮实际消费的深度需求并纳入图及池的现有界限。替换储存继续按真实 GPU 完成退休。 [图资源与保留](../../../../Docs/Architecture.md#graph-resources)
- Runtime 只在完整正常场景提交、消费存活且对应输出获接受后确认新数组与该 View 的有效阴影事实。裁剪、失败、取消和仅有前缀票据均保留原接受记录，后续当前请求按匹配键重试。每路的接受事实带自己的 View、Scene 和票据，面板同时可读当前 CPU 规划与原因。计时沿用最新正常图及匹配提交的非阻塞场景区间。 [统计增量](specs/render-frame-statistics/spec.md) · [诊断合同](../../../../Docs/Architecture.md#diagnostics)

## Risks / Trade-offs

| 风险 | 对应检查 |
| --- | --- |
| 过滤或偏移把 tap 推出有效覆盖，造成接缝、漏影或错误层读取 | 用可区分层内容和独立覆盖预期检查最大核、点光面边/角、窄聚光及级联交界。用不同深度的已知九点给出独立过滤期望。 |
| 灯行或层复用错配，或者只有前缀提交就发布深度 | 在同一受光图验证不同颜色的灯、换灯、两个 View 和接受失败后的恢复。核对实际图层读写、Shader 包形状及已接受票据。 |
| 离屏和主 View 同时用多组时增加深度准备及数组字节成本 | 复用冻结帧资源，按计划与实现的各自口径度量。固定场景比较 Off、Hard、PCF，区分生产及复用帧。 |

## Migration Plan

沿现有构建入口产出更新的 Shader 包和反射，按帧边界完整代启用。场景 JSON 沿用 v1，旧文件直接由原入口打开。会话阴影初始关闭，用户可以在原 Renderer 面板切换；关闭后普通受光模式继续以完整灯表出图。 [构建入口](../../../../Docs/Build.md)
