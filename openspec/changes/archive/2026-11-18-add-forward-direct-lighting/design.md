# Design

## Context

`KHR_lights_punctual` 已由 glTF 入口校验并随模型实例进入 Scene。`SceneFrameStorage` 已在安全槽内提取标准强度、世界位置及发射方向，`ViewWork` 用稳定视锥返回可见灯索引。Forward 当前只消费 Primitive 工作集，光栅颜色入口输出材质预览，`ForwardView` 已提供同轮 `preExposure`。 [glTF 入口](../../../../Engine/Asset/GltfData.cpp) · [SceneFrame](../../../../Engine/Renderer/Prepare/SceneFrame.cpp) · [可见性](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [当前颜色入口](../../../../Shaders/Passes/Forward/Color.frag)

## Goals / Non-Goals

本包把同一份 View 工作集从灯记录送到可读回的预曝光工作色。Forward 按完整集合调用统一的灯与表面响应，后续路径可替换候选选择和表面取得。D07-T04 负责环境、DFG 与 IBL，D07-T05 负责显示，D07-T06 负责灯光编辑，D09–D11 负责阴影及其他路径。 [范围](proposal.md) · [光照聚合](../../../../Docs/Architecture.md#lighting)

## Decisions

### 1. 在冻结的 View 工作集上编码一次

- `Renderer/Lighting` 消费 `PreparedView.lights` 及其借用的 `SceneFrame`。沿用现有稳定视锥及影响球结果，灯心在外但影响范围相交的有限灯继续保留，无截断的局部灯始终作为候选。方向灯独立成段，后接完整局部灯段，各段保留输入索引顺序。场景增删、显隐、世界位置和发射轴变化沿既有 Scene 内容代传播。 [灯列表合同](../../../../Docs/Architecture.md#light-preparation) · [场景变更](../../../../Engine/Scene/SceneTransactions.cpp)
- 从 Scene 世界刚体朝向读取方向灯及聚光的发射轴，对应灯节点局部 `-Z`。局部灯位置、距离和 range 的量纲为米。场景缩放可改变节点世界位置，强度、range 与锥角仍保留源单位和值。编码保留线性 RGB 和 lux 或 candela，曝光交给当帧着色。 [物理输入](../../../../Docs/Architecture.md#physical-inputs) · [KHR_lights_punctual](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual)
- 在进入 GPU 接口时检查实际使用的 FP32 字段。写入的正强度和显式 range 须为有限正规数，避免设备清零改变光度或截断语义。强度允许零，range 的零编码专用于缺省。聚光在 double 中用 `q = (2 sin(θ/2))²` 编码内外锥端点。设外锥弦长为 `cOut`、FP32 epsilon 为 `ε`，用 `d = 8ε` 估计方向编码与求值的弦长误差。窄化后的端点间距须至少为 `128 × ((2cOut + d)d + ε cOut²)`，为归一化过渡保留约 7 位分辨率。超出编码能力时拒绝灯表并指出灯实体或字段，Scene 保留已接受的创作数据。 [逐灯接口](specs/punctual-lighting/spec.md) · [灯行编码](../../../../Engine/Renderer/Lighting/PunctualLights.cpp)

### 2. 使用现有生成接口和图内上传

- 在 `StaticForward` 权威 schema 用固定容量 `std430` 只读 record 数组交付灯表。`color/intensity`、`positionHigh/range`、`direction/innerChordSquared` 和 `positionLow/outerChordSquared` 各占 16 字节。1 MiB 的单块预算对应 16384 条。方向灯由第一段计数区分，局部段以有效弦长平方区间标记聚光，点光的两个端点为零。CPU 从生成接口取得容量与 stride，检查计数、字节数及设备范围。超容量时整个受光候选给出原因。 [接口格式](../../../../Engine/Shader/Interface/README.md) · [单块预算](../../../../Engine/Shader/BuildSupport.hpp) · [GL 活动范围](../../../../RHI/OpenGL/CommandListGraphics.cpp)
- 灯表作为 `ShadeOpaque` 的 PerPass 只读资源绑定，前置深度保持原活动资源集合。`ForwardView` 随本次快照交付两段数量、透视眼点或正交固定观察向量、投影类型及 `preExposure`。颜色与深度共用裁剪表达式与 `invariant`。权威 schema 同时生成位置 IO、常量、灯行及绑定，`StaticForward` 和 `StaticDepth` 用对应 ABI 打包。 [现有 schema](../../../../Engine/Shader/Interface/StaticForward.json) · [共享顶点路径](../../../../Shaders/Geometry/StaticVertexFetch.glsl)
- CPU 以 double 将图元包围盒中点变到世界空间，取其 FP32 值为光照基点 `A`。对象行保存平移差 `T − A` 的高位和余量，以及线性矩阵系数窄化的余量。若基点或平移差超出 GPU 编码范围，由对象行准备返回对应原因。顶点直接求 `M × position + (T − A)`。Dekker 乘积拆分和 TwoSum 保留 FP32 乘加余量，将相对偏移拆成两部分分别插值，基点平坦传递。片元从灯或眼点高位减去同一个 `A` 和偏移高位，再合入减法及编码余量。这样使平移和目标位置围绕同一编码原点，在包围盒变化时仍保留可见表面的重合关系和可分辨的小距离。Depth 和 Color 共用原裁剪表达式。对象行按对象身份和编码后的世界基点区分，按世界变换判断重用。纯相机移动且可见索引一致时可复用对象行及灯表。 [对象行编码](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [顶点位置](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [逐灯计算](../../../../Shaders/Lighting/Punctual.glsl)
- 依照现有完整缓冲上传和 GL 活动块范围校验，固定容量绑定覆盖整个反射块，首次或灯数据改变时完整初始化。`ShadeOpaque` 声明读该次 `UploadBatch` 写出的 `ShaderRead` 版本，绑定使用图许可的相同范围。诊断模式、零灯和纯 Unlit 绘制复用一份初始化的空表及零计数；无 Draw 时延续已有附件清除。 [上传与图组装](../../../../Engine/Renderer/Paths/Forward.cpp) · [绑定准备入口](../../../../Engine/Renderer/RenderGraph/PassPreparation.cpp)
- 普通灯表随 `ForwardFrame` 槽持有。用 Scene 身份、内容代及完整有序可见索引判断是否可复用已接受的行。纯曝光只改变当帧 View 常量。取消、资源暂不可用或提交失败不推进该表的接受状态，重新取得安全槽后重试，其他在途槽继续持有各自数据。容量和永久编码失败返回拒绝，暂时上传不足沿既有等待路径，视口继续按已有规则使用安全 UI。 [帧槽合同](../../../specs/frame-slot-lifecycle/spec.md) · [Runtime 接受边界](../../../../Runtime/Frame.cpp)

### 3. 共用表面求值与直接光公式

约定 `D` 为灯面向场景的单位发射方向，`L` 为片元朝灯的单位方向，`r` 为距离。`V` 在透视时由世界位置朝眼点求得，正交时取相机固定反视线方向。`wRange` 在显式 range 内为 `max(1 − (r/R)^4, 0)`，到边界归零，缺省 range 时为 1。聚光计算 `q = dot(D + L, D + L)`，与 `2 × (1 − dot(D, −L))` 等价。先取两方向的差可保留近轴信号。把 `(qOut − q)/(qOut − qIn)` 限幅到 0–1 后平方，得到相同的余弦过渡。 [项目距离窗](../../../../Docs/Architecture.md#physical-inputs) · [Filament 直接光](../../../../Docs/Reference/Filament/Lighting.md#f11) · [Khronos 角衰减参考](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#inner-and-outer-cone-angles)

| 灯类型 | L 与入射量 |
|---|---|
| Directional | `L = −D`，入射量为 lux 强度，随位置保持常量 |
| Point | `L = (lightPosition − worldPosition) / r`，入射量为 `I × wRange / r²` |
| Spot | 同局部灯方向，入射量为 `I × wRange × wSpot / r²` |

- `EvaluateDirect` 回用共享 `EvaluateCoreBrdf` 的漫反射、GGX、完整 Smith 与 Schlick，乘一次正半球入射余弦、线性灯色、该灯入射量和 View 预曝光。局部光与片元重合时方向无定义，跳过该灯，正的可用距离保持反平方。有限遮罩或角衰减为零时直接交付零。`EvaluateLighting` 累加所有匹配的方向及局部贡献。当前直接光使用核心单次散射，真实 DFG 补偿在 D07-T04 消费。D09 的阴影可见度接到逐灯响应，当前有效可见度为 1。 [核心 BRDF](../../../../Shaders/BRDF/CoreBrdf.glsl) · [不透明入口](../../../../Docs/Architecture.md#opaque-shading)
- 受光片元只取样一次有效基色，纹理梯度及几何粗糙度导数在覆盖分歧前可用。与 Depth 共享 MASK 判定，保留切线、镜像及双面合同。覆盖后构建一次 `PixelParams` 供所有灯复用。AO 属于后续间接光输入，直接光与自发光不受其衰减。灯贡献和线性自发光各乘一次同轮 `preExposure`，零灯受光表面的工作色为自身预曝光发射，非发光表面为零。保留 RGBA16F 可表示的超白值，有限性及格式上限在工作色写出边界处理。 [静态表面合同](../../../specs/static-surface-evaluation/spec.md) · [正式 Forward 行为](specs/static-forward-rendering/spec.md)

### 4. 从现有 View Settings 到正常接受帧

在现有 `PreviewMode` 后追加 `Direct Lighting`，初始模式为 Base Color。该模式对受光 OPAQUE/MASK 使用直接光，Unlit 保持合法覆盖下的标准基色。诊断模式、`BLEND` 的报告、背景和 Reverse-Z 填充沿原路径。Mode 只属于本次 View 运行请求，保留既有 v1 文件结构。显示通过 `ForwardDisplay` 限幅到 display-linear 的 UI 采样边界，HDR 数值在此之前读回。灯表拒绝有独立的 View 状态和视口占位，在帧诊断保留实体和字段。View Settings 的已接受模式来自实际接受的颜色代，新模式和曝光由实际采样并接受的正常图确认。 [View 模式](../../../../Engine/Renderer/View.hpp) · [显示入口](../../../../Shaders/Passes/Forward/Display.frag) · [视口确认](../../../specs/editor-viewport-output/spec.md)

## Risks / Trade-offs

- 点光单位、距离窗、灯轴符号和重复余弦会直接改变 HDR。使用可独立求解的照度、固定表面和原生读回核对三类灯及叠加。
- 定长 SSBO 支持现有 Shader 和 GL profile，初次或数据变化时的完整上传会消耗有界缓存和额度。按帧槽和实际候选复用已确认的表，核对超限、零计数和失败恢复。
- 颜色入口增加世界位置、导数和资源读取，共享 MASK 及深度覆盖可能回归。用同一标样比较合并与前置深度方式，核对旧诊断和模式接受。 [最小验证](tasks.md#verification)

## Migration Plan

正式构建同步生成新接口及颜色、深度配套包，加载时继续核对 ABI。已有 glTF 灯和 v1 场景在 Direct Lighting 模式通过当前读写入口直接使用。构建说明记录模式、覆盖范围和当前显示入口。回退使用同一代的源码、生成接口与配套包。 [构建与运行](../../../../Docs/Build.md)
