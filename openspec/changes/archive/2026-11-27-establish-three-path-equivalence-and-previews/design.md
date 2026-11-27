# Design

## Context

规划基准为 `v2.0@78cb8e8975af59f0d1a7bca52b9701026a5ee2cd`，即已归档的 D11-T04。`ForwardOutput` 已携带显示前 `workingColor`、配对 D32、渲染布局、实际路径和可选的最终 `CoreGBufferOutput`。它的 `colorPass` 在 Deferred 中标识辅助重画，核心屏幕光照可能存活而该 ID 为空。成本口径须涵盖真实生产范围。普通主视口当前只解析 `DisplayColor`，Runtime 的 `latestScene` 在主 View 接受时优先记录主 View，仅离屏 View 接受时记录该路。`latestGpu` 为界面保留最近的 Ready 区间。 [提案](proposal.md) · [Renderer 输出](../../../../Engine/Renderer/Paths/Forward.hpp) · [主图与回执](../../../../Runtime/Frame.cpp) · [统计和轮询](../../../../Runtime/FrameStatistics.hpp)

当前导入和 `PrepareMaterial` 的产品 lit 材质均走核心路由，Unlit 独立走辅助路由。最终 G3 已定义 lit `ForwardExtension` 的分类，实际工作量按有效材质行计数。把该分类的预览读回与本次真实场景的扩展占比分别登记。 [材质准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [最终表面规格](../../../specs/core-gbuffer/spec.md#requirement-resolve-the-nearest-opaque-surface-across-material-routes)

## Goals / Non-Goals

新增预览只需要当前普通受光帧所具备的输入。表面分类和量化仍归既有 GBuffer 合同，预览的选择、显示与占位归编辑器会话，正常出图和接受记录归对应 View 与 Runtime。阶段比较基准为 Windows x64/OpenGL 的单样本 Physical Lighting、None AA。D11-T04 的 Direct Lighting、None/FXAA 和事务切换证据按变更影响复用。标准材质扩展、透明、屏幕效果、MSAA、TAA 及其它 backend 使用各自工作包的验收入口。 [路径统计和画质](specs/render-frame-statistics/spec.md) · [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)

## Decisions

<a id="preview-flow"></a>
### 1. 从已选普通子图消费类型化表面

在现有 Workspace 增加默认关闭的主 View 表面预览，单次提交至多有一个所选通道。选择和面板显隐是会话诊断状态。普通视口与预览各自登记实际会绘制的 Image 需求，沿用封存 DrawData 的 token 和像素裁剪判断。预览需要同一普通主视口确实消费当前图的场景颜色，因此主视口隐藏时显示原因。这样只需为当前已选子图加一个读取者，保持 T04 的计划选择、选中路径的资源集合和正常采样确认条件。额外生成前向 GBuffer 会引入第二份表面及其预算，当前用现成深度覆盖前向检查。 [现有 UI 活性合同](../../../specs/editor-frame-graph/spec.md#requirement-cull-unused-editor-producers-from-actual-ui-reads) · [预览规格](specs/editor-viewport-output/spec.md)

1. Editor 以稳定 `(ViewId, SurfacePreviewColor)` 角色保存独立 token、所选通道和当轮需求。普通 `DisplayColor` 保持现有 token 和语义。Runtime 根据实际选中的 `ForwardOutput`、普通 UI 是否采样及当前 Graph/Scene/View/布局和诊断请求装配预览输入。状态需要明确区分当前帧的预览可用性与上次确认信息。
2. Renderer 的全屏 Preview Pass 读取配对的 D32 或最终 G0–G3。前向只提供当前最终 D32，沿现有 ShadeOpaque 或已有 depth 生产，不为诊断额外安排预通道。Deferred 使用经过 SurfaceAux 后的 G1/G3 和配对的 G0/G2，按照图声明获取正确版本。读取使用源渲染栅格的坐标与半开有效矩形，明确纹理原点，初始化留边。D32 预览使用深度 aspect 且本 Pass 只采样深度，避免同时绑定为深度附件。 [样本边界](../../../../Docs/Architecture.md#geometry) · [图资源合同](../../../../Docs/Architecture.md#graph-resources)
3. 使用有限的产品配方表达深度浮点读取与混合 GBuffer 读取。前者读取 D32 单样本，后者读取 sRGB/浮点 G0–G2 和整数 G3。通道选择属于有界参数。片元按像素取数，复用现有分类常量与 oct 解码。每个 Pass 只声明其配方需要的范围。输出为初始化的普通单样本颜色目标，与主场景显示独立，UI 沿现有线性合成和一次 sRGB 存取的合同消费它。 [着色包与生成接口](../../../../Docs/Architecture.md#shader-build) · [现有解码库](../../../../Shaders/Material/CoreGBuffer.glsl) · [UI 合成](../../../specs/editor-ui-rendering/spec.md#requirement-composite-straight-alpha-ui-in-the-linear-display-domain)
4. Runtime 只在本图把预览色版本交给相应 FrameSlot UI 绑定。丢弃、代变更及安全 UI 回到诊断占位，预览本身不留存图句柄或纹理历史。图依赖形成表面生产到 Preview 到 UI 的顺序，旧提交的纹理和绑定随真实完成退休。普通颜色仍由其实际输出读者和回执确认。 [视口解析](../../../../Docs/Architecture.md#editor-output) · [UI 纹理注册](../../../../Editor/ImGui/UiTextures.hpp)

| 通道 | 已定义的显示含义 |
|---|---|
| Reverse-Z D32 | 近端 1 为亮，0 为暗。有限远面和清除值的区分使用 Deferred 的路由视图。 |
| G0 基色、AO | 同一次 sRGB 取样的线性 RGB 或线性 AO 灰阶，有效域为核心资格像素。 |
| G1 法线、粗糙度、金属度 | 世界单位着色法线映射为 `0.5 * N + 0.5`，粗糙度和金属度以灰阶显示。所有已覆盖路由交付法线和粗糙度，金属度限核心。 |
| G2 自发光 | 有效核心的预曝光 RGB 用逐通道 `x / (1 + x)` 映射为预览色，维持同次预曝光；图例标明映射。 |
| G3 路由 | 整数取数，`0x0001` 核心、`0x0002` Unlit、`0x0004` lit `ForwardExtension`，零为无覆盖。显示色与图例一一对应。核心专属通道在辅助覆盖处用明确背景。 |

本轮最多增加一张与渲染栅格对应的颜色目标。按现有设备采样/附件能力和图预算核对该目标和配方。普通计划优先，只有当前活需求且可满足专属额度时才登记预览工作。Shader 缺失、准备额度等待、格式不支持及预览专属预算拒绝在封存时降为占位，使用现有有界诊断。候选包核对活动片元资源、浮点与整数数值类、颜色输出配方和兼容代，D32 的资源格式和 depth aspect 由图的类型化输入及绑定配方核对。活需求驱动预览原生准备，旧正常角色仍沿原代和就绪合同交付。整图声明、准入或执行失效沿已有回退和真实票据合同，列表开启后维持一次图执行。 [着色准备](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [图失败回退](../../../specs/editor-frame-graph/spec.md#requirement-compile-an-independent-safe-ui-fallback-before-recording)

编辑器先封存预览画布及历史票据。着色器需求登记与实际绘制共用栅格拟合、帧缓冲比例和像素裁剪。UI 准备将普通几何放入连续前缀，并保留互斥的诊断几何。通过裁剪检查的候选分别接受 UI 几何准入，超出预算时将该候选的拒绝状态传给 Runtime。构图按已声明的纹理和场景写入、本轮选中几何及所需预览参数预检，上传只包含普通前缀与选中的图像或标签。所选图像因 UI 几何预算不可绘，或原因标签因几何或上传额度不可绘时，保留空画布和已准备的主路、离屏路及普通 UI。构图从本次 UI 准备值复制被省略标签的原因，标签绘制与省略记录共用同一正文来源。提交接受后沿用帧诊断，将完整票据放在可截断的正文之前。正常图封存后继续按存活计划精确准入。错误路径的非 ASCII 码点以 Unicode 转义显示，沿用封存前已准备的字形与当前图字体版本。 [UI 合成](../../../../Editor/ImGui/UiRenderer.cpp) · [帧准入](../../../../Runtime/Frame.cpp)

<a id="quality-protocol"></a>
### 2. 以匹配的工作色和深度判定等价

使用现有 Runtime、场景和 View 请求、RenderGraph 执行、OpenGL 提交与完成路径。不计时的图用显式消费者读取合成天空后的预曝光 RGBA16F、配对主 D32 和需要的最终 G3 或预览颜色。明确真实导出和 Copy 的资源版本、格式、深度 aspect 与合法状态。通过 Event/完成后用现有可用的读回能力取得原始值并处理实际行距和行序。D32 如采用已经验证的原生图出口，等到同票据完成并遵循当前宿主与状态交接。每路记录本轮的 Scene/View、内容及请求代、相机与矩阵、有效渲染和显示布局、有效效果、曝光、实际路径、生产 Pass/Shader 代、图代与接受票据。历史回退、单纯请求、未存活的 Pass 和诊断输出自身均排除在本轮验收样本外。 [读回合同](../../../../Docs/Architecture.md#readbacks) · [D11-T04 接受证据](../2026-11-27-enable-deferred-path-switching/tasks.md#final-verification)

画质分组固定几何和有效材质，核对覆盖、D32、分路标样和全画布 RGB。用工作色的几何 alpha 与深度、有效矩形及独立见证交叉定位表面，Deferred 另核对最终 G3。天空仍使用已有的覆盖规则。冻结样例让参与跨路比较的遮挡具有明确最近者，并在独立位置保留正交 D32=0 的覆盖见证。共面辅助胜出沿 D11-T02 的既有约定检查。对于固定输入的覆盖、Unlit 和背景，要求相同的可见性、有限值和对应的工作色；D32 在本轮同输入之间按数值匹配，并以几何解析点核对取样位置。D10 的 Forward/Forward+ 回放以数值一致为判据，零的两种符号视为相同。 [D10 参考](../../../../Docs/Performance/ForwardPlusReference.md#shadow-reference) · [共面辅助合同](../../../specs/core-gbuffer/spec.md#requirement-compose-auxiliary-color-at-the-depth-of-the-final-surface)

对含 GBuffer 编码的核心，在正式比较前登记逐组的绝对底限、相对项、适用像素和误差来源。校准应从 D11-T01 的字段精度、D11-T03 的独立交点和受光误差、工作色的末次量化推导。对相同条件的 D11-T03 固定见证可复用 `2^-12 + 0.003 × |参考 HDR|`；其它材质、投影或阴影输入按其实际精度冻结判据。阴影的变化范围用固定的遮挡/亮区标样和地图/接收位置核对，轮廓差异独立定位并计数。报告全图及相应区域的各通道最大绝对差、RMSE、非有限数、超限像素、覆盖与深度差异。任何修改判据的需求都先定位首次差异并记录可独立复算的依据及会失败的对照。显示域截图可确认预览的 UI 消费，工作域差分直接使用原生线性读回。 [GBuffer 字段证据](../2026-11-26-implement-core-gbuffer-codec/tasks.md#final-verification) · [重建证据](../2026-11-27-add-deferred-lighting-subgraph/tasks.md#final-verification)

| 分组 | 最小冻结输入与独立判据 |
|---|---|
| 混合表面与预览 | 复用 D11-T04 的混合输入，加入必要的独立字段见证。用同网格的有效实例覆盖、临界 MASK、核心前后遮挡、Unlit 与远面/空白、不同显示和渲染尺寸的留边验证可见性与读回。复用一份输入核对各颜色预览转换及缩放/切换。用临时类型正确的整数标样验证真实场景未出现的 lit 扩展类别。 |
| 阴影与环境 | 重建 D09/D10 的 640×480、PCF 512、3 组 11 层及 IBL/天空输入。用登记的遮挡区、无遮挡接收区、MASK 边缘、天空和配对 D32 核对共用画质。沿相同输入复用或运行 Off 与去环境对照以确认两类贡献。 |
| 无范围灯与退路 | 让同一受光工作集同时命中有紧凑候选的格和实际 overflow 格，保留方向灯、无范围局部灯及有限点光/聚光。利用共享查询入口的匹配观测区分正常、溢出与无有效格，记录实际访问灯行。灯贡献负对照应能检出遗漏。Forward 提供完整可见灯表的正式 HDR 参考。无限远透视、零近平面正交及大坐标覆盖沿用匹配前置证据，仅对尚未被当前组合证明的边界补读回。 |

输入重建及许可沿 D09/D10 的已登记来源，固定原生测试材料、脚本及命令归 `build/d11-t05/`。候选遍历、清除后复用及投影边界的观测可借用前置夹具与共享 Shader 查询，观测包、修正与正式包分别留存身份。 [已准许资产](../../../../Assets/README.md#forward-参考场景) · [阴影标样](../../../../Docs/Performance/ForwardShadowReference.md#quality) · [多灯灯序](../../../../Docs/Performance/ForwardPlusReference.md#many-lights-fixture)

<a id="workload-scope"></a>
### 3. 从接受计划生成有统一口径的附件和路由统计

Renderer 在 `ForwardOutput` 及现有接受统计所用的 View 槽上记录实际生产范围。从存活计划和纹理描述取本轮 `主 D32 / G0–G3 / 不透明 RGBA16F` 的逻辑存储。按逻辑纹理身份去除 AddSky 等产生的版本重复，以及 Deferred 两个输出角色指向同一 D32 的重复。使用已有格式字节与图存储描述，按完整栅格计当帧单 mip、单层、单样本的逻辑附件规模。纯受光且表面都存活时，D32 加工作色为 12 B/像素，Deferred 的 G0–G3 另为 18 B/像素。640×480 下分别对应 3,686,400 B 和 9,216,000 B 的理论合计，可作为对账判据。只有实际存活者入账。整图物理槽、池占用、实际分配仍由各自 owner 给出。 [图计划](../../../../Engine/Renderer/RenderGraph/GraphCompiler.hpp) · [图池预算](../../../specs/render-graph-resource-pool/spec.md)

带宽相关指标取上述纹理在该 View 从表面准备到天空合成的存活 Pass 声明。按声明的子资源与格式估算整范围逻辑读取字节及写入字节。每个 Pass 内同一范围、同一方向只计一次，显式 `Sampled`、只读深度和 Load 计读，颜色及深度写用途计写。后续 Pass 再访问同一范围时按该次访问计。记录指标的完整范围口径，GPU 压缩、缓存、实际覆盖及片元级访存量留待有相应测量时报告。阴影附件、显示链、预览色、UI、Copy 读回及整图 RHI 上传有各自范围。预览的额外目标字节和是否存活单独可查。 [现有统计合同](../../../specs/render-frame-statistics/spec.md) · [本次统计要求](specs/render-frame-statistics/spec.md#requirement-attribute-opaque-attachment-and-route-workload-to-the-accepted-view)

路由分母为当前 View 的可绘不透明项总数，一个实例表面项计一次。分子分别为可绘项中 lit `ForwardExtension` 和 Unlit 的计数，随该 View 的已接受工作量发布。比例以实例表面项计。在原生质量组可从最终 G3 另算 Deferred 的可见覆盖比例，给出分子、分母和有效矩形。统计快照保持选定 View 来源，沿当前主优先、仅离屏接受时取其来源的规则。无新正常接受时继续显示原记录。 [接受快照发布](../../../../Runtime/Frame.cpp) · [路由当前实现](../../../../Engine/Renderer/Draw/StaticDraw.cpp)

<a id="benchmark-protocol"></a>
### 4. 形成可复算的首次三路径成本比较

1. 固定同一 Release 构建及有效配置、单 View、640×480、单样本 None、Physical Lighting、已确认 VSync Off。沿 D10 多灯序列比较 0、32、256 个有效局部灯，关闭阴影、IBL、天空，保留原冻结输入和曝光。再以同一 256 灯输入限制 Forward+ 和 Deferred 的有限索引额度，得到实际全表退路的对照，复用该输入的 Forward 基准。每组先核对正式 HDR/D32 和有效工作量，保留独立的分支观测结果。所有路径用相同宿主、UI 条件和计时边界。采样时关闭新增预览和质量读回。
2. 使用现有 Runtime 启动设置、状态入口和向产品 View 派发的冻结输入，复用已有 `RenderFrame` 与 `FrameStatistics`。采集和复算驱动留在 `build/d11-t05/`，与普通构建的 Runtime 和正式 Shader 入口分离存档。记录实际 exe、库、Shader 和夹具身份。成本组使用交错路径轮次，每轮在路径接受并暖机 6 个有效帧后采 24 个正常有效帧，计划三轮。CPU 分别记录场景和 View 准备、UI、构图、编译、执行及其工作合计；FrameSlot 和 PresentWait 单列。GPU 使用现有整图非阻塞区间，标明包含相同配置的 UI；阴影、后处理等若有分项，注明真实边界。
3. 在正常采集和后续轮询的同一次运行内登记可用区间的原始终态。复用 D10 的开发期观察手段，在 `latestGpu` 聚合之前留存同轮全部解析结果。按接受票据及 Scene/View/内容与请求代、实际路径、布局、有效 Shader 代、场景和灯组身份对账。在相同接受点记录该 View 实际消费的 Froxel 输入及其有效额度、格数、溢出数和完整灯行数，使每个样本能与相应分支见证匹配。统计 CPU/GPU 各自 median/p95、有效样本数及缺测类别。失败、缺测和票据身份不匹配有明确原因。只追加受影响且需要补足的测量组。
4. 每组对账实际可见灯行、Froxel 总格、overflow 格、退路像素比例见证及一次退路完整遍历行数，附上实际 Draw、绑定、上传字节、图槽/分配、附件逻辑字节/读写指标以及 lit 扩展与 Unlit 的项数占比。报告本轮观察到的扩展规模与可证范围。所有速度结论以签收的质量和分轮数据为前提，无法分辨的差别照实记载。 [D10 测量协议](../2026-11-26-verify-forward-plus-hdr-and-lighting-cost/design.md#benchmark-protocol) · [设备与统计口径](../../../../Docs/Architecture.md#benchmark)

<a id="delivery"></a>
## Migration Plan

预览随产品 Shader 包构建交付，初始关闭，场景 v1 和普通默认路径沿用已采纳的配置。恢复普通出图由关闭诊断消费或回到兼容代完成。最终接受后将可重建场景参数、判据、实际三路径分布、样本身份和适用范围整理入 `Docs/Performance/ThreePathReference.md`，供后续扩展、屏幕与时域工作包沿用。签收明细和原始记录引用保留在本 change 的 tasks，开发计划在阶段出口完成后登记范围与状态。 [文档归属](../../../../Docs/Architecture.md#project-docs)

## Risks / Trade-offs

- G3 清除与 D32 远平面可能同时为零。以真实覆盖、最终整数分类、有效矩形和表达明确的数据通道核对预览，选取能区分这两种含义的标样。
- 切换产生同尺寸但不同来源的颜色。预览匹配本图的实际生产者、请求和角色，资源按各自票据退休。以切换、resize 和仍在途的同一序列一起回归。
- 前向与 Deferred 的量化或阴影采样可能出现差异。固定解析标样、误差来源与负对照，报告遮挡边界及连续区域的读回差异。
- 异步计时可能同轮回收多条。对账聚合前事件，分别核对工作量与原提交身份；用一次交换或缺失标签的负对照验证统计过滤。
