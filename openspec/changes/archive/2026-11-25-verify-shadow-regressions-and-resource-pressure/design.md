# Design

## Context

规划基准为 `v2.0` 的 `5b69ac039d03dabbe41373109bb52738b05ee5f8`（D09-T04）。Renderer 为每个 View 规划灯组与所需内容，Forward 在存活分支逐层写入或只读导入 D32。Runtime 按正常接受结果交付深度和该 View 的统计。图池和 RHI 依据最后一次实际访问的完成及持有关系回收存储。实施时以实际构建输入和原生回放清单确认身份。 [灯与阴影计划](../../../../Docs/Architecture.md#light-preparation) · [阴影生成与查询](../../../../Docs/Architecture.md#shadows) · [图池](../../../../Docs/Architecture.md#graph-resources) · [在途退休](../../../../Docs/Architecture.md#retirement)

共用静态几何、MASK 覆盖与过滤比较可对照项目固定的 Filament 分析。帧接受、计时结果和在途引用各有自己的完成条件，可对照固定的 NVRHI 分析。实测通过条件以 Physara 当前的 OpenGL 合同为准。 [Filament 阴影](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f9) · [NVRHI 完成与回收](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n10)

## Goals and scope

以同一正式静态 Forward 生产链完成 D09 出口验收。固定输入兼顾可解析判定的覆盖形状、灯组压力和后续路径可重放的 HDR。使用已采纳的单队列 OpenGL、静态 OPAQUE/MASK、常规阴影深度及 Off/Hard/PCF。Forward+ 与 Deferred 的输出等价由后续工作包使用这份参考分别验收。 [D09 出口](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05) · [共享不透明着色](../../../../Docs/Architecture.md#opaque-shading)

## Decisions

### 1. 固定输入与生产入口

- 优先复用已归档探针的场景和独立判据。组合基准由获准使用的 Forward 参考场景派生，增加可解析的接收板、薄片、带明确孔位的 MASK 和共用网格的实例。光源可用方向和颜色区分，画外遮挡物能在固定屏内接收区产生变化。保存变换、有效材质和贴图、观察位置及编辑顺序，登记实际打开的文件、生成参数、来源许可和 SHA-256。 [参考资产与许可](../../../../Assets/README.md#forward-参考场景)
- 使用真实 GL host、正式加载的 Shader 包和 `Runtime::RenderFrame`。固定质量参考使用单主 View，压力序列另启一路 `OffscreenViewRequest` 验证失败与恢复的独立归属。D32 层及预曝光 HDR 通过现有图权限和 staging 取得，必要的观测代码及回放入口放在 `build/d09-t05/`。运行入口可使用隐藏窗口。`--poll-limit` 用于控制宿主运行，采样以接受票据为准。 [Runtime 帧入口](../../../../Runtime/Frame.hpp) · [构建与运行](../../../../Docs/Build.md#运行) · [图读回合同](../../../specs/render-graph-execution/spec.md)
- 前置归档是复用入口。只有正式源码及本地变更、构建配置、实际 Shader 包和可执行产物、资产与回放步骤、设备驱动及观察口径对应时才复用结果。已有局部证明可覆盖相同局部风险，新的组合场景和压力时序需有当前输入的证据。 [计划](../2026-11-24-prepare-shadow-plans/tasks.md#final-verification) · [CSM](../2026-11-24-add-stable-directional-csm/tasks.md#final-verification) · [局部阴影](../2026-11-24-add-spot-point-shadows/tasks.md#final-verification) · [Hard/PCF](../2026-11-25-add-hard-pcf-shadow-visibility/tasks.md#final-verification)

<a id="accounting"></a>
### 2. 按实际身份核对用量和访问

| 来源 | 观测与口径 |
|---|---|
| View 准备与阴影计划 | 按 Scene、View、计划版本及灯 identity/generation 记录有效边长、组、区域代、所需内容代、逻辑层跨度、字节和拒绝原因。计划字节按规划的层跨度计算。 |
| Forward 与 RenderGraph | 关联该帧接受、资源版本和物理层映射。逐所查组核对 `DepthWrite` 的 Clear/Draw 或匹配已接受只读导入、`ShaderRead` 及实际采样范围。读回独立声明 `CopySource` 和根。核对实际执行顺序、子资源状态和应有的同步。 |
| 执行与图池 | 分别记录已接受图所用 D32 描述符字节与设备池实际计入的逻辑对象/字节。池容量按物理对象去重，记录保留、物化、命中、录制、在途、外部持有、空闲及淘汰的来源。原生创建/删除、Clear、Draw、绑定和上传按实发命令计。 |
| Runtime 与 RHI | 将每路 View 当前计划、最近接受统计、整图快照、最后实际使用 ticket、Event/Fence 及 Timer 分别配对。提交接收和窗口呈现仅推进各自事实。完成观察与持有解除共同决定再次借出或删除。 |

常规深度使用近端 0、远端 1，D32 清除值 1，写入比较 `Less`，查询比较 `<=`。主颜色的深度采样仍按主 View 的 Reverse-Z 解读。分组的原生读回与共享查询、受光结果各自确定独立预期。 [深度及采样规格](../../../specs/shadow-rendering/spec.md) · [帧和阴影统计](../../../specs/render-frame-statistics/spec.md) · [访问与同步](../../../../Docs/Architecture.md#barriers)

<a id="verification-matrix"></a>
## 验收规格

| 检查 | 固定场景和通过条件 | 判定依据 |
|---|---|---|
| V1 · CSM 与静态覆盖 | 接收标样跨级联内区及非零过渡带。沿光空间 X 方向平移相机，从同一吸附格内走到相邻格。投影尺度固定，格内对同一世界标样的对齐保持，跨格按整 texel 改变。带内在相邻两级可见性不同的样点检查权重和混合值，覆盖外查询为 1。原生 D32 的实心、MASK 孔、薄片双面、镜像绕序和各实例位置分别可辨。移走画外 caster 或关闭其投射时，屏内受影响标样恢复可见，专门留作空层对照的深度回到 1。 | [阴影计划](../../../specs/shadow-planning/spec.md) · [阴影生产与采样](../../../specs/shadow-rendering/spec.md) |
| V2 · 聚光边缘与点光六面 | 旋转聚光灯，在名义外锥两侧及有效投影内选择标样，锥内有遮挡与无遮挡，锥外中性。点光读回六个有独立身份的 D32 层，使用非对称深度图案验证各面方向、相邻面、同轴幅时的 X、Y、Z 优先顺序和三面角。边和角上的 PCF 核完整地落在所选扩展面内。对无 range 的一盏灯，在有效阴影距离外仍有直接光贡献，所查阴影中性。 | [局部深度与查询](../../../specs/shadow-rendering/spec.md) · [完整灯集合](../../../specs/punctual-lighting/spec.md) |
| V3 · 比较与 HDR | 比较深度 0.5 对应 4 个 0.25 与 5 个 0.75 的 D32 核时，中心为 0.25 的 Hard 得 0，PCF 得 5/9。中心为 0.5 时 Hard 得 1。使用匹配的双尺度接收偏移证据，保留置零偏移必失败的对照结果。对同输入的普通 Direct、Physical 与实际诊断比对可见性和受光 HDR，固定或隔离环境、自发光及 Unlit 标样。测得的变化能归到被遮挡的逐灯直接项，记录预曝光 HDR、主深度和显示值各自的格式和观测点。 | [Hard/PCF](../../../specs/shadow-rendering/spec.md) · [Forward 消费](../../../specs/static-forward-rendering/spec.md) |
| V4 · 多灯准入与图访问 | 一个四级方向光、一盏聚光、一盏点光在设备允许 512 texel 和 11 层时，8 MiB 下依次准入四级及一层，点光以预算原因整体拒绝。16 MiB 下完整准入 11 层。继续增加覆盖有效的局部灯，使阴影需求超过有效层或字节上限且灯表仍在容量内。逐灯核对顺序和完整组，确认代表性拒绝灯继续贡献直接光。实际采样层都可追到定义深度及自身灯。用同类无消费图验证零存活深度 Pass 和零阴影存储物化，独立深度读回通过声明的版本与范围取得实际 D32 内容。 | [准入和版本](../../../specs/shadow-planning/spec.md) · [活性](../../../specs/render-graph-compilation/spec.md) · [精确访问](../../../specs/render-graph-execution/spec.md) |
| V5 · 换代、停用与退休 | 先接受并暖机，删除已准入灯后补入有不同遮挡标样的灯，在相容布局中重用至少一个逻辑区域。旧、新灯身份和区域/内容代可区分，新颜色及读回同新组对应。随后执行 Off/On 和尺寸 A→B→A，含一次附加 View 无效尺寸再恢复。Off 的新接受 View 无阴影生产和采样，正常灯光仍出图。失败 View 保留自己的最后正常接受结果，正常 View 独立推进。按下述时序证明旧深度在途保护、受控压力、完成后的安全复用或淘汰和容量收敛。 | [深度接受](../../../specs/shadow-rendering/spec.md) · [池和外部持有](../../../specs/render-graph-resource-pool/spec.md) · [提交观察](../../../specs/opengl-completion-queries/spec.md) · [每路统计](../../../specs/render-frame-statistics/spec.md) |
| V6 · 画质与成本参考 | 按 [测量协议](#benchmark-protocol) 交付可比较的像素、有效质量和成本。冷帧、固定静态暖帧与 V4/V5 的压力转折可以分别追溯到对应场景、图访问和实际计数。静态暖帧及同布局的 Hard→PCF 保持零阴影重绘，明显的复用失效或用量异常须有可定位解释及复测证据。 | [阴影内容版本](../../../specs/shadow-planning/spec.md) · [测量合同](../../../../Docs/Architecture.md#benchmark) |

<a id="in-flight-replay"></a>
### 在途时序

1. 在 V5 的替换前，对要被替换的已定义深度记录最后实际消费它的 Graphics ticket、物理对象身份及范围。通过声明过的 staging 复制保存其可辨标样，并把该复制计入实际最后使用。释放被测旧槽的图外持有后，在同一真实 fence 零超时仍为 Pending 的窗口中执行替换及兼容槽需求，验证在途引用自身足以阻止再次分配和删除。其他仍有合法持有的同队列只读导入可按原提交顺序读取旧版本。
2. 用观察到的退休/持有边界验证池在硬上限及空闲保留额度内处理压力。若容量因在途或外部持有受阻，记录现有拒绝或背压及来源。需要定向隔离该条件时，通过图池现有构造参数设置容量，复用同一原生 D32 场景。真实完成后读取旧 staging 与新接受结果的不同标样，检查错层、覆盖和提前删除。
3. 由真实完成观察推进到最后使用 ticket，解除读回和导出持有后验证兼容复用、淘汰或正常退出。连续尺寸往返到达相同需求时已用及空闲容量有界。按归因检查池和原生操作，允许有效缓存留存。压力观测要求同一在途票据，允许在本地回放调度代表性负载或暂缓观测，并记录受控环节及实际原生结果。样本在压力动作前已完成时重新取得代表样本。 [图池前置证据](../2026-11-23-complete-render-graph-resource-pooling/tasks.md#final-verification) · [传输与查询前置证据](../2026-11-22-complete-opengl-transfer-queries/tasks.md#final-verification)

<a id="benchmark-protocol"></a>
## 测量协议

1. 使用单主 View、640 × 480、固定曝光、Physical Lighting、None AA、1.0 render scale、固定环境、相同 UI 状态及有效 VSync Off。冻结模型、材质、灯、相机和同一编辑序列中的加载态。基准选择可同时交付 11 层的混合场景，在 16 MiB 请求下采 Off、Hard 512、PCF 512 和 PCF 256 四组。每组登记请求和有效质量、实际灯组、场景/相机/渲染尺寸与环境身份，并确认两档分辨率均保有同一组灯及覆盖范围。
2. 先从同一质量配置的匹配接受帧读取预曝光线性 HDR、主 Reverse-Z 深度、阴影层和接收标样。以投影及已知遮挡几何固定 ROI，保存遮挡/亮区、过渡与缝接样点的覆盖率、HDR 差及误差统计。按格式精度和独立参考明确数值容差、采样范围及误差汇总方式，再运行比较。用 Off 和逐灯标样分离直接贡献，分辨率/过滤的视觉差异随实测成本记录。同质量路径对照须重放同一有效质量、场景、相机、曝光、环境与灯集合。
3. 记录各档首次接受帧的创建、写层及耗时。各帧槽、资产和图池暖机后，各档用固定接受帧窗口取样，起步每档 72 个接受帧。交错采样，保持计时窗口内的 UI、观测开销、读回策略一致，像素读回和详细原生追踪在窗口外以匹配身份回放。V4/V5 在压力转折记录实际工作及容量峰值。实际执行时记录设备、驱动、构建、预热数、采样数、接受、跳过和失败数据；仅在噪声、有判别价值的样本缺口或输入变化时补受影响的组。
4. 每组汇报 CPU 准备、构图、编译、执行、UI 及可用匹配 GPU Ready 时长的 median/p95，槽等待和实际交换等待单列。按原始 ticket 关联 Timer，保留 Pending、缺失和有效样本数，采样期间维持正常非阻塞轮询。同时记录实际生产与复用层、批次、原生 Draw、绑定、上传次数/字节、创建及缓存变化，并按 [用量口径](#accounting) 对账。门槛以匹配画质、正确工作和有界用量判定，耗时分布作为实测参考。
5. 将已验收的复现入口、数据摘要、格式与容差和实测适用范围整理为一份可重用的 Forward 阴影参考，供 D10-T05 和 D11-T05 对齐同输入 HDR 与阴影工作量。按架构 12.2 在 `Docs/Performance/` 留存后续比较所需的配置、重建参数和代表样本，原始 CSV/图像和临时回放留在工作包目录。归档 tasks 记录实测证据及参考位置。 [Forward+ 比较](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05) · [Deferred 比较](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)

## Risks / Trade-offs

- 灯几何体无覆盖也会得到无阴影结果。多灯标样先核对接收相交、投影可交付及直接光可辨，再验证预算拒绝和同灯恢复。
- 阴影层与主深度、计划字节与池的物理存储、接受与完成各自有独立的观测值。样本带 Scene、View、图、资源代和票据，并显式记录来源及去重范围。
- 同步获取像素会改变原生时序。先保留在途和计时证据，随后从匹配提交的已完成读回验证内容。每项检查以能区分错误结果的最小输入结束。
- 前置历史结果的支持范围可能随产品与设备输入变动。保留身份、失败和缺测原因。当前设备无法证明的合同保持待验收，复用条件由签收版逐项核对。

## Migration Plan

1. 在工作包临时目录完成固定回放和证据收集。现有合同出现偏差时记录输入、观测和最小修正，复测受到影响的消费者。
2. A 按实际结果交接，B 独立审查及注释，A 整改自检，B 复核。A 对 B 签收输入执行最终 verify。 [独立审查闭环](../../../../AGENTS.md#review-loop)
3. 按最终规格范围执行同步判定和原生归档，更新 D09 出口、D10 前置及参考入口。签收和证据留存后按流程重置本轮问题单。
