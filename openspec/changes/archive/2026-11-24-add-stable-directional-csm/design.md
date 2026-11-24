# Design

## Context

`ViewWork` 从冻结的 `SceneFrame` 和 `CameraSnapshot` 交付每 View 的阴影计划。`ShadowPlan` 已有主方向光优先级、区域代和所需内容代。它以单个方向光接收段为单位筛选 caster，分配层号可以离散。`PlanDraws` 目前从主可见索引准备驻留及 Draw，`ForwardFrame::Declare` 按选中的 View 声明共享上传和各路子图。这三个边界分别承接级联计划、画外几何准备和深度生产。 [提案](proposal.md#why) · [CPU 计划](../../../../Engine/Renderer/Lighting/ShadowPlan.hpp) · [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [Forward 构图](../../../../Engine/Renderer/Paths/Forward.cpp)

## Goals / Non-Goals

### Goals

- 从同次计划交付可追溯的级联投影、caster、区域与受图授权的深度视图。
- 在正式 View 出图链上以同一深度证明级联选择、过渡和遮挡。静态帧复用实际已接受的深度组。

### Non-Goals

D09-T03 负责局部灯投影与边界，D09-T04 负责 Hard/PCF 过滤、质量界面和共享光照查询，D09-T05 负责跨灯型和预算压力。D09-T02 的 GPU 消费者是 Forward 的方向光级联诊断。 [工作包顺序](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02)

## Decisions

<a id="settings-and-splits"></a>
### 每 View 请求与级联区间

沿用 `ShadowSettings` 的会话身份和请求校验。在缺省 50 m、1024 texel 及关闭规划的基础上，方向光采用缺省 4 级、透视对数权重 0.5、过渡半宽比例 0.05。类型化请求接受 1–4 级、有限权重 `[0,1]` 及有限过渡比例 `[0,0.25]`。诊断开关属于同一 View 的独立会话显示选择。级数和连续参数通过生成的数据接口交付，程序目录按深度生产和诊断的结构差异提供有限入口。 [View 准备](../../../../Docs/Architecture.md#view-preparation) · [完整程序代](../../../../Docs/Architecture.md#plan-readiness)

从稳定相机取 `n = nearPlane` 和 `f = min(cullingFar, shadowDistance)`。以 `t = i/N` 求内部分割。透视取 `lerp(lerp(n, f, t), exp(lerp(log(n), log(f), t)), lambda)`，正交只取 `lerp(n, f, t)`。两端直接使用 `n`、`f`。相机在相应投影 profile 内才进入计算。空区间或在交付精度下无法形成有限严格递增端点时，报告方向光级联不可用。

相邻名义分割点 `s[i]` 两侧各扩展 `transitionFraction * min(s[i] - s[i-1], s[i+1] - s[i])` 米。每级用包含其两端过渡区的 receiver 切片拟合和筛选，首尾限制于 `[n,f]`。只有内部切分产生过渡。查询先按稳定相机轴取米制线性视深，再在过渡带内按线性权重混合两次 visibility。范围端点及等值深度的约定见规范。 [阴影合同](../../../../Docs/Architecture.md#shadows) · [GPU 行为](specs/shadow-rendering/spec.md)

<a id="stable-fit-and-allocation"></a>
### 稳定拟合、成组分配与内容版本

在刚体相机空间中用含固定采样余量的切片八角确定包围球。使用近远截面中心的中点及覆盖八角的半径即可。光空间 XY 以球半径确定方形半宽，再留出吸附位移和单点采样边界的固定余量。吸附步长为实际方形边长除以有效栅格边长。用由灯方向确定的正交轴和固定世界格点吸附球心，XY 尺度只依赖切片形状和质量档。既有 `DirectionalVolume` 的紧投影范围仍可用于保守筛选。 [采纳的稳定化](../../../../Docs/Architecture.md#shadows) · [Filament 固定快照分析](../../../../Docs/Reference/Filament/VisibilityAndCommands.md#f4)

从同一冻结场景的全部可投射 Primitive 取候选。用接收切片的光空间范围与采样余量筛选 XY，沿发射轴保留位于接收区域上游及相切的候选。Z 以 receiver 和选中 caster 的保守区间建立，预留数值及偏移所需余量。现有光空间描述以 `+Z` 沿发射方向，生成 clip 时按右手观察绕序解释 XY，Z 沿光线由 0 增至 1。交付值包括可表示的参考点、轴、投影范围及每 texel 米数。绘制和查询共享该坐标合同，NDC 转纹理使用左上原点。 [现有范围检查](../../../../Engine/Renderer/Lighting/ShadowVolumes.cpp) · [坐标约定](../../../../Docs/Architecture.md#conventions)

拟合同时核对世界网格的半 texel 分辨率及交付精度。将接收区间及 caster 合并范围分别换算为各世界轴的相对分量上界，按每条光轴实际参与点积的分量估计 FP32 舍入。近远面至少各留一个 texel。当 Z 跨度增大时，按深度轴的舍入和常量写入偏移加大近远面余量，XY 继续以完整接收切片确定尺度。将高低位原点相对相机解释后，逐轴核对上述覆盖。任一级无法表达到所需精度时，整组返回 `InvalidVolume`。

按现行优先级选择设备和字节预算能接受的边长。N 级占 N 个逻辑层，完整申请成功后才发布该灯的映射。兼容的灯与级编号保留逻辑区域，空间内容改变时推进对应内容代。CPU 计划允许各级分别失效，其内容依赖包括实际光空间变换、深度覆盖、模型及实例版本和有效 MASK 输入。变更相机后可重算选级数据，吸附、深度和 caster 依赖相同的层仍可复用。 [分配与版本规范](specs/shadow-planning/spec.md)

<a id="caster-draws-and-depth"></a>
### Caster 绘制与深度偏移

在共享 UploadBatch 封存前，以每级 caster 索引准备静态 Draw。跨级复用可共享的几何、对象、材质和纹理版本，每级保有自己的绘制范围。深度程序共享实例行解析、局部到世界变换、UV 及 `StaticCoverage.glsl`，OPAQUE 和 MASK 保持真实覆盖，适用的单面逆绕序和双面行为沿用现有光栅键。只服务于阴影的输入按实际覆盖需求准备，仍由对应图访问维持上传活性。 [几何合同](../../../../Docs/Architecture.md#geometry) · [驻留规范](../../../specs/static-gpu-residency/spec.md)

阴影深度的顶点布局只依赖实际 MASK UV，可共用已有完整材质行和绑定。准备过程中真正执行的对象、材质和灯行编码由同槽累计，跨级与跨 View 的命中共享已编码行。单路拒绝时保留该轮已经发生的 CPU 编码，上传仍以图内实际存活和接受为准。

复用现有世界位置的基点和高低位偏移来构造相对光空间位置，诊断同时用同一表面位置计算稳定的相机视深。常量 caster 偏移缺省取半个世界 texel，除以光空间 Z 跨度后沿常规深度的正向加到写入值。斜率偏移使用现有 RHI raster state 的正向 slope factor，首版为 1。查询使用同一基础投影的接收深度。常量偏移随级联数据交付，光栅状态属于深度角色。 [静态顶点入口](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [OpenGL 光栅状态](../../../../RHI/OpenGL/ContextGraphics.cpp)

<a id="graph-production-and-retention"></a>
### 实际层、图权限与接受版本

为每个被实际诊断消费的 CSM 组使用单 mip、单样本 D32 数组。按级联序把 CPU 逻辑区域映射到该数组的连续物理层，保留逻辑层和分配／内容代用于关联。聚光和点光的区域继续参与 T01 逻辑预算。各级使用单层深度附件并声明 Clear/Store，包含无 caster 的已分配级。消费方声明同一生产版本中实际采样的各层，D32 depth aspect 及 mip/layer 范围沿 `PassPreparation` 绑定核对。

Shader 使用最多四个固定 `texture2D` 槽接收数组的单层视图，沿现行 `GraphicsBaselineV1` 走 `texelFetch` 的后端 dummy sampler 组合。着色器按有效级数访问对应层。少于四级时，仍活动的补位槽可绑定同一已初始化层。计划为空时走不需要阴影采样的路径。这样可以保持精确子资源授权，并使用已有二维采样 ABI。整段数组视图还需要 canonical 维度校验和原生数组 SRV，当前逐层消费已有足够的能力。 [单层视图](../../../../RHI/OpenGL/Bindings.cpp) · [当前 Shader profile](../../../../Engine/Shader/Spirv.cpp) · [图接口](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp)

诊断查询的变换矩阵与其余查询参数分成两个生成常量块，各自满足正常帧图的单块常量上限，绑定同一次 View 准备的数据。

现有图编译用单个采样视图校验 ShaderRead 出口。CSM 的整组只读交接通过 D32 数组的显式层组能力逐层证明同格式采样，其出口范围仍须全部有定义，实际 Pass 仍受单层能力限制。设备从现行 D32 数组深度附件和逐层采样能力建立该行。只有不可变的数组出口可采用此规则。 [图执行增量](specs/render-graph-execution/spec.md)

D32 采样边界沿用图中的 `ShaderResource | DepthRead`。后继图继续从已接受的实际入口状态导入，图尾回到该深度采样状态。

首版将连续深度组作为不可变 `ShaderRead` 输出保留，只在诊断帧图确实选中该 View 的实际输出时建立保留。`View` 持有最近已接受组。缓存以设备、View、Scene、有效布局、各级区域与内容代、深度生成程序及固定光栅语义核对。命中时重新导入该组并刷新本次查询常量。任一生产依赖变化时为当前组重新生产完整数组，旧快照由真实录制和提交保活。整组重建的额外工作有四级上限，粒度足以兑现静态空闲复用，逐层原地更新需要额外的写权限和读者协调。

CPU 计划和设备匹配时，以已接受组实际使用的深度变体核对候选 Shader 代，先准备本次查询常量。选代后摘要仍相同的组直接只读导入，跳过 caster 驻留、批次与上传预留。摘要变化时，在封存共享上传前回到该 View 的预算继续准备重绘。深度组存储的变体来自实际覆盖与光栅状态，保持在同一 CPU 内容代内可用。

补做 caster 准备的分配失败由所属 View 结束本轮候选，在图声明前沿原有失败退路处理。两路共用的生产者仍按剩余候选的实际图访问决定活性。深度组同时保存生产时的 caster 跳过摘要。新生产和只读导入采用同一摘要，与本轮主表面自身的降级一起交付。

完成图执行且正常提交被接受后，依据所属 View 的实际活性、导出和回执确认当前深度代。准备失败、图拒绝和仅前缀提交保留重试资格。诊断关闭、阴影禁用或内容失去匹配时，View 释放对旧组的持有，池按真实使用退休。GPU 数组由现有图池按实际物理占用计账，CPU 逻辑账本继续反映所有准入层及其跨度。 [跨帧输入](../../../../Docs/Architecture.md#graph-resources) · [提交验收位置](../../../../Runtime/Frame.cpp)

<a id="diagnostic-consumer"></a>
### Forward 诊断及退路

通过现有 Renderer 面板提交会话开关，借用正常 Forward 几何和 ViewOutput 链。诊断颜色编码级联编号及过渡权重，明暗来自对应样点的 visibility，沿现有线性诊断显示配置输出。选择区间所用的视深取 `dot(cameraForward, surfacePosition - cameraOrigin)`。采样使用同次级联投影生成的 UV 和比较深度，比较可用层的原始单点深度。范围外返回全可见，停止访问无效区域。 [诊断规范](specs/static-forward-rendering/spec.md)

诊断的请求与上次正常接受的诊断身份均可观察。规划禁用、无区域或可预知的设备限制使用可用的普通 Forward 并显示原因。图或提交失败沿既有待确认和上次有效输出协议处理。新增程序和绑定随现有准备预算及帧边界的完整代交付。目录将层数、范围、bias 和调试数据留在运行期参数，保证不同 View 各自选择。 [计划就绪](../../../../Docs/Architecture.md#plan-readiness) · [视口输出合同](../../../specs/editor-viewport-output/spec.md)

诊断片元的活动资源同时核对材质覆盖和级联采样配方。候选包在准入时校验，冷启动的包在创建原生对象前校验。同布局场景色也携带阴影请求代，正常 UI 在构图前匹配该代。诊断准备或上传准入失败时，安全 UI 可显示上次可读颜色，并标明当前原因与旧图实际模式。

离屏附件预检按渲染与显示尺寸核对必需附件和普通预览所需的 FXAA 附件。若预算只容纳级联诊断，View 准备须确认主可见集可以消费诊断，阴影规划须准入方向光组，才交付新分配。该条件在区域分配和缓存发布前生效。布局不可用时释放数以该 View 上次交付的分配为准，重复拒绝空分配时分配代保持稳定。若本轮已取得规划结果或规划失败，阴影摘要随布局拒绝交付方向光的实际原因及规划拒绝计数。Forward 规划继续核对所选路径使用的实际附件。

## Risks / Trade-offs

- 吸附后的切片超出深度有效范围，或过渡带只由一侧级联生产。用明确的切片覆盖、边界余量和两级原生样点证明各自可读。
- 方向轴变换、镜像剔面或 bias 方向错误会让近 caster 缺失。用遮挡位置已知的非对称场景，同时读回 D32 层和主 Reverse-Z 深度。
- 层地址复用、Shader 换代或提交未完成会使旧内容和当前诊断配对错误。用精确层授权、范围读回及图接受后的身份对照验证。

## Migration Plan

既有场景通过 v1 文档入口打开，规划和诊断使用会话默认值。启用规划后可在同一会话启用级联诊断，关闭诊断恢复该 View 当前请求的显示模式。设备、生效配置或程序代不满足当前请求时沿正常帧退路提供可定位状态。 [场景持久化合同](../../../specs/scene-document-persistence/spec.md)
