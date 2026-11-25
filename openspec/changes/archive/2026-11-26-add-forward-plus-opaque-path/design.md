# Design

## Context

正常帧通过 `ViewWork::Prepare` 取得同一 `SceneFrame` 的每 View 工作集，经 `ForwardRenderer::PlanView` 汇总两个 View 的实际 Pipeline 需求，帧前推进及选定 Shader 代，由 `FinishView` 完成帧内准备。`ForwardFrame::Declare` 封存一次场景上传并组装表面、着色、天空和显示，正常图的实际消费及提交决定 ViewOutput 是否获接受。 [Runtime](../../../../Runtime/Frame.cpp) · [Forward 子图](../../../../Engine/Renderer/Paths/Forward.cpp) · [计划准备](../../../../Docs/Architecture.md#plan-readiness)

D10-T02 提供 `FroxelUpload` 和 `FroxelGraphInputs`，现有产品颜色入口仍消费完整灯表。产品角色的 PerView 准入目前依据只含 `b0` 的布局。接入 Forward+ 同时改变产品活动资源、按 View 的准备需求以及图内资源入口。 [上传接口](../../../../Engine/Renderer/Lighting/FroxelGpu.hpp) · [Shader 准入](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [D10-T02 验收](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification)

## Goals / Non-Goals

- 让现有 Renderer 的正常帧执行已就绪的 Forward+ 不透明策略，兼容当前 None/FXAA、主视口和离屏 View，共用现行资产版本与图生命周期。 [路径组装](../../../../Docs/Architecture.md#paths) · [显示合同](../../../../Docs/Architecture.md#display-output)
- D10-T04 承接编辑器路径选择、待选计划与当前有效计划的持续执行事务及历史兼容性处理。D10-T05 承接阶段 HDR 和多灯成本验收。后续工作包各自交付 Deferred、透明、扩展材质与时域 profile。 [阶段规划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t04)

## Decisions

### 将路径选择固定在现有 View 和帧准备中

给 View 的 owner 线程提供带类型的运行时 `Forward`/`ForwardPlus` 请求，缺省为 Forward。Runtime 对主视口和离屏 View 走同一个选择规则，将路径与本轮相机、布局、预览模式和 AA 一起封存。当前路径只影响普通 Direct Lighting 和 Physical Lighting 的受光颜色角色。材质诊断和已成功解析的阴影诊断仍走原入口。阴影诊断无可用目标时按原规则恢复普通模式，然后用该 View 的路径决定候选。 [View](../../../../Engine/Renderer/View.hpp) · [当前 View 工作集](../../../../Engine/Renderer/Prepare/ViewPreparation.hpp) · [View 准备增量](specs/view-preparation/spec.md)

`PlanView` 根据实际绘制和显示需求登记颜色、深度、阴影、天空和显示键，`FinishView` 使用该帧已选定的同一代取得所需对象。集合缺项复用现有有界的 Shader 准备状态，拒绝使用已有的带原因结果。Ready 表示本轮能声明完整依赖和绑定配方，图预检仍须按读权限物化实际绑定。Runtime 只在帧边界为本轮图选用准备好的策略，最终确认沿用 ViewOutput 的正常采样与提交条件。为已接受记录保留实际计划的路径身份及所属 Shader 代，供 D10-T04 使用。未准备或未接受时复用有读取资格的旧输出。 [计划准备](../../../../Docs/Architecture.md#plan-readiness) · [提交确认](../../../../Docs/Architecture.md#history-commit)

路径属于 View，因此主视口与离屏 View 能在同一帧各自选择。用现有 Renderer 的颜色角色选择表达策略，保留同一个 `PrepareSurface` 和不透明 Pass 组装。独立 Renderer 会重复驻留、批次和图生命周期管理。 [子图合同](../../../../Docs/Architecture.md#paths)

### 复用表面和逐灯求值，配对片元坐标与稳定分箱

在现有 `Color.glsl` 的共享受光入口选择 `EvaluateLighting` 或 `EvaluateFroxelLighting`，覆盖、导数、表面、DFG、IBL、自发光和最终写色仍由同一代码负责。复用 D10-T02 的方向灯前缀、绝对灯行归并和溢出退路。Filament 固定版本在片元中由 Froxel 记录取得局部灯索引并交还共享表面响应。Physara 的数据保持现有 32 位行号和预算合同。 [共享颜色入口](../../../../Shaders/Passes/Forward/Color.glsl) · [Froxel 求值](../../../../Shaders/Lighting/FroxelLighting.glsl) · [固定版 Filament 局部灯入口](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_punctual.fs#L180-L247) · [光照聚合](../../../../Docs/Architecture.md#lighting)

Forward+ 在逐灯求值使用的 `LightingSurface` 上以相同的高低位差值求相机相对位置，再用生成常量中的相机旋转和稳定投影取得相对有效矩形的像素及米制 `-viewZ`。透视按正视深除法，正交使用线性投影并允许零近平面。直接查询稳定坐标省去对画布原点和抖动的往返转换，两种入口使用同一套 cell 边界。大坐标下 `gl_FragCoord` 对应的光栅深度可能落在另一层，这时仍以实际参与照明的表面定位候选。无法保守投影或超出网格时沿用完整灯退路。覆盖、反向深度与前置深度使用原有共享光栅路径。 [相机数据](../../../../Engine/Renderer/Prepare/CameraSnapshot.hpp) · [查询合同](../../../specs/froxel-light-preparation/spec.md) · [光照聚合](../../../../Docs/Architecture.md#lighting)

高低位眼点的剩余编码误差由 CPU 在世界轴上逐分量求绝对值并求和。旋转保持距离，因此该上界同时覆盖视空间各轴上的查询偏移。将它加入有限局部灯的保守跨度及半空间判定，可保留跨 XY 或 Z 边界的候选。准确可编码的眼点无需这项放宽。 [分箱实现](../../../../Engine/Renderer/Lighting/Froxel.cpp)

即使眼点编码精确，片元合并高低位时也可能舍入。从补偿作差各次合并的 TwoSum 余量得到算术误差上界，将相机相对位置的区间投影到稳定画幅。只有区间的投影极值及近远视深归入同一格时才消费紧凑候选。跨格或无有效格使用完整可见灯集合，逐灯光照和反向深度仍按共享求值及光栅入口交付。 [相对位置](../../../../Shaders/Geometry/LightingPosition.glsl) · [片元查询](../../../../Shaders/Lighting/FroxelLighting.glsl)

点光和聚光的灯行按轴将源位置的前 24 个有效位存入 FP32，余下 29 位存入原低位字段宽度的无符号数。Shader 用高位的符号和指数将尾数拆成两项 FP32。该布局保持 64 字节行距及 16,384 行容量，整块仍在生成器的 1 MiB 上限内。任何不能保持源值的非零编码分量使本 View 的灯表准备失败，沿用 `SceneLightTableRejected` 和可读旧输出。CPU 分箱沿用源灯位及眼点和变换误差上界。 [灯行编码](../../../../Engine/Renderer/Lighting/PunctualLights.cpp) · [灯表主规格](../../../specs/punctual-lighting/spec.md)

共享逐灯入口先在表面基点附近抵消高位，再合入灯位第三项及相对作差的有符号余差，之后使用同一米制位移计算距离窗、距离衰减和入射方向。正常格与完整列表使用同一入口。 [位移补偿](../../../../Shaders/Geometry/LightingPosition.glsl) · [逐灯求值](../../../../Shaders/Lighting/Punctual.glsl)

### 为有限产品角色定义一致的 CPU 绑定合同

为 Direct Lighting、Physical Lighting 带 DFG、Physical Lighting 带 IBL 三个受光组合各登记现有无阴影及共享阴影查询角色的 Froxel 颜色对应项。沿用已登记的顶点布局、绕序及深度固定状态。增加的角色共享 shader include 和生成接口，产品清单、目录、正常安装路径、依赖观察、包加载及旧代保留沿用现有入口。Hard/PCF、灯数量、灯强和预算仍由准备结果或运行常量表达。天空、后处理和各深度角色沿用已存在的键。结构键使全灯角色保持原有活动资源，用动态开关为两策略共用同一颜色角色会使其绑定依赖扩大。 [变体目录](../../../../Engine/Shader/ProductVariants.cpp) · [产品构建](../../../../Engine/CMakeLists.txt) · [Shader 构建合同](../../../../Docs/Architecture.md#shader-build)

全灯保持现有 PerView `b0`。Froxel 颜色角色在同一空间增加活动的 `b1` 和 `t0` 至 `t2`，保留原所需的灯、阴影和环境绑定。打包冷准备和 `AdmitCandidate` 依据同一角色合同核对必需项、额外活动项、阶段及生成类型。当前未被选择的产品角色同样进行包级合同校验。实际需要的布局和原生 Pipeline 随 View 需求在 OpenGL owner context 内按原额度准备，避免 Forward 启动受未经请求的 Froxel 原生准备或设备限制影响。 [生成接口](../../../../Engine/Shader/Interface/StaticForward.json) · [角色准入增量](specs/shader-generation-reload/spec.md)

### 由同一场景上传写版本提供 Froxel 绑定

对可能受光的 Forward+ View 请求 CPU 列表，并以实际颜色角色作为 GPU 消费依据。固定布局确定的 Froxel 字节数和写入笔数在规划该 View 时一同预留，按已有的先主路、后离屏顺序准入。该路收尾就绪时按设备的布局、块大小、范围和对齐检查，用 `FroxelUpload::Prepare` 将等额预留换成拥有型写入。未就绪或被拒时撤销该路预留，仍有需求的其它候选保留自己的预留和共享生产者。单格 overflow 已具备正常消费资格。普通 Forward 以及不消费 Froxel 的角色不需要这组预留。 [图上传](../../../../Docs/Architecture.md#uploads) · [Froxel 规格](../../../specs/froxel-light-preparation/spec.md)

主路在收尾阶段撤回并实际归还额度时，Runtime 对因场景上传额度失败的附加 View 用已冻结的工作集重试一次。新增原生键并入本轮两路需求，保留主路仍待准备的键及其状态。场景上传额度失败经 View 准入报告。补备当前 Shader 代沿用本帧剩余额度，后续无新计划的预热仍可推进本轮欠缺的键。候选 Shader 代继续在原帧边界发布。 [帧调度](../../../../Runtime/Frame.cpp)

冷包的打开、长度及内容检查在读取边界交付角色拒绝，ABI、活动接口或 profile 不兼容在对应准入阶段交付同样范围的失败。该角色的固定状态键共用首因，仍计入实际需求。活动代继续用剩余额度准备其它角色。新源请求、选定新代或该角色离开需求可清除记录，重新请求时读取已修复的包。资源不足、系统失败及原生设备错误沿用既有中止规则，候选代仍需完整准备才能发布。 [原生准备](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp)

`ForwardFrame::Declare` 的实际场景上传生成唯一写版本。用 `FroxelUpload::Resolve` 从该图的版本表取得同一组写版本，`FroxelGraphInputs::DeclareReads` 和 `Bind` 分别向实际颜色 Pass 声明范围并取得预检绑定条目。准备拒绝时立即撤回本路，未入图的候选在封存前撤回。独占缓冲按帧内 View 位置识别，共享缓冲及纹理按存活 View 的准备代保留。撤回该次准备代的使用证明，便于后继重新准备已失去生产者的未接受存储。根据留下的写入、仍需初始化的冷页以及实际显示预留重算额度，共享 LUT 只计一次。已封存的图继续以真实依赖裁剪，已发前缀依其票据保活。 [NVRHI 上游记录及上传分析](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n6) · [Graph 规格增量](specs/static-forward-rendering/spec.md)

## Risks / Trade-offs

- 全灯与 Froxel 的 HDR 相同也可能由每个片元都走完整灯退路造成。验收同时核对真实图读绑定和一个能够辨认正常格查询的原生样点，并覆盖带留边的非对称矩形和正向视深。 [V2](tasks.md#verification-scope)
- 颜色角色与冷包、热重载或图预检配方脱节会导致换代后失效。使用一致准入合同及冻结代，分别验证常规打包交付和一组角色绑定错误。 [V1](tasks.md#verification-scope)
- 固定容量的活动块会消耗上传和驻留额度，索引预算只控制有效条目。以设备限制和真实字节拒绝无法交付的候选，一路被拒时保留另一路仍需的工作。以按 View 的统计区分已准备索引、实际上传和已接受结果。 [帧统计](../../../specs/render-frame-statistics/spec.md) · [V3](tasks.md#verification-scope)

## Migration Plan

常规构建一同生成并交付角色目录对应的包。既有 v1 文档和默认入口继续使用 Forward。调用方可在下一帧边界恢复 Forward 请求，现有场景、兼容资源及在途提交各按原拥有关系继续使用。
