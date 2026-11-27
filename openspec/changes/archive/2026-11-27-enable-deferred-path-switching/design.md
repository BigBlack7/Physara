# Design

## Context

`Runtime::PlanOpaqueView` 以有效路径规划共用工作，`ForwardRenderer::SelectOpaquePath` 只为候选切换前向颜色键和 Froxel 需求。D11-T03 通过 `ViewResourceNeeds::deferredLighting` 进入正式 Deferred 子图，该入口会拒绝同时传入 `alternatePath`。Deferred 的表面路由、四个 GBuffer 附件、D32 消费和核心颜色角色必须与所选策略一起准备。 [提案](proposal.md) · [正常帧入口](../../../../Runtime/Frame.cpp) · [当前规划与选择](../../../../Engine/Renderer/Paths/Forward.cpp)

`View::Publish` 已能确认正常候选和采用有效路径的匹配帧，`OpaqueProduction` 已有核心表面、辅助和 Deferred 颜色生产身份。当前自动 Froxel 与部分跨 View 的上传优先级只识别 Forward+，主 View 的编辑、文件枚举和菜单也只包含两条路径。 [View 确认](../../../../Engine/Renderer/View.cpp) · [工作集](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [实际生产合同](../../../specs/deferred-opaque-rendering/spec.md)

## Goals / Non-Goals

**Goals**

- 让两路现有正常调用入口从同一冻结输入选择已就绪的不透明计划。所选路径、表面、着色、资源需求和接受记录来自同次决定。
- 让兼容性与资源用途由 Renderer 解析，View 拥有请求、确认及历史资格，Runtime 编排帧槽和提交，Editor 读取状态并提交字段意图。

**Non-Goals**

- D11-T05 承接三路径画质等价门槛、附件可视化和成本。D14/D15 承接屏幕效果生产及 TAA 历史消费。可选多样本策略需要独立的采样、光照、resolve 与成本合同。 [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05) · [效果与时域计划](../../../../Docs/Developments/D14-D15-ImagePipeline.md)

## Decisions

### 由固定入口选择完整的不透明策略

沿用架构 6.1 的固定枚举映射，在现有 Renderer/Paths 中确定成对的表面准备和不透明着色入口。先解析所需用途，再封存选中的路径和类型化数据。表面与颜色阶段、图版本、资源请求和 `OpaqueProduction` 使用同一选择。 [路径组装](../../../../Docs/Architecture.md#paths) · [View 路径规格](specs/view-preparation/spec.md#requirement-assemble-one-complete-opaque-subgraph-from-live-demand)

| 策略 | 表面入口 | 不透明颜色与依赖 |
|---|---|---|
| Forward | 普通前向按需求准备前置深度或附加表面 | 普通几何颜色、完整灯表，未提前写出的深度随颜色交付 |
| Forward+ | 与 Forward 共用表面入口 | 几何颜色使用当前 View 的 Froxel 及完整灯表退路 |
| Deferred | 现有 Core GBuffer、可选 SurfaceAux 与最终单样本 D32 | 核心屏幕照明读取最终 GBuffer/D32，辅助颜色以同一 D32 只读 Equal 接续 |

三策略继续沿 `SurfaceOutput → LightingInputs → OpaqueOutput` 的合同接入现有数据。沿用已有类型及版本，表面只含真实后继所需输出。SceneFrame、相机、批次的公共输入、阴影、环境和材质驻留在现有模块管理。天空和显示由公共收尾编排，对无受光核心或只有天空的 View 仍初始化工作色。独立表面和颜色附加表面沿用其显式需求。Filament 固定参考中的图活性与声明/执行分界用于核对资源归属。 [表面与辅助合成](../../../specs/core-gbuffer/spec.md) · [现有照明](../../../specs/deferred-opaque-rendering/spec.md) · [图参考](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f7)

### 解析实际 profile 和可交付的资源用途

View 保留类型合法的路径与 AA 意图。帧前由 Renderer 使用当前模式、布局和设备能力解析两个可能策略的显示与表面需求。沿用 `ResolveDisplaySettings`。Physical Lighting 用单样本 AcesSdr 与实际 None/FXAA，Direct Lighting 用 LinearClamp/None。材质预览、测试图及实际阴影诊断仍走其产品入口。诊断只确认自己的输出，挂起的受光请求留待正常受光图。 [能力规格](specs/view-preparation/spec.md#requirement-resolve-the-supported-configuration-for-each-opaque-strategy) · [显示合同](../../../specs/display-post-processing/spec.md) · [View 显示配置](../../../../Engine/Renderer/View.hpp)

由现有设备与图能力数据核对所选计划的目标数量、尺寸、格式、单样本、整数 G3 读取、D32 读取用途和附件预算。Deferred 的 GBuffer 每像素 18 B，D32 和现有 RGBA16F/显示附件分开记账。前向附加表面继续按实际额外深度计费。按实际描述校验表面与颜色的布局和绑定，沿用图在真实资源访问处的验证。配置限制、暂缺额度和原生角色拒绝分别保留具体原因。单样本能力表显式给出 Deferred+MSAA 为 false 和 TAA 未启用；当期持久化 AA 值域仍为 None/FXAA。关闭的 AO 和屏幕反射使用 `screenAO=1`、零反射及零置信度。 [设备能力](../../../../Docs/Architecture.md#capabilities) · [核心布局](../../../specs/core-gbuffer/spec.md) · [图资源合同](../../../../Docs/Architecture.md#graph-resources)

### 在现有预算内分别准备有效与待选计划

复用 `ViewWork` 的共享场景提取和每 View 工作集。任一可执行策略存在当前受光 Froxel 需求时，用当前 View 的冻结相机和灯生成一份可共用的 CPU 列表；已选策略的实际受光才领用其图上传。有效与候选可以共享几何、材质、环境及相同键的对象。按实际路由从批次推导候选的 GBuffer、辅助 Equal、核心屏幕照明及各自阴影/环境角色，沿用产品目录和冷、热准入，去重后交给已有帧前 native 额度。待选准备保持有限，已有效和其它 View 的实际工作有明确预算归属。 [批次准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [Shader 准备与预算](../../../specs/shader-generation-reload/spec.md) · [Froxel 生产](../../../specs/froxel-light-preparation/spec.md)

选择时将路径、实际布局、角色与资源作为一个整体。两路先为有效策略预留当前实际需要的私有写入，包含 Froxel 四块及核心受光所需的新屏幕常量。屏幕常量的已接受字节匹配时可直接读取。首次或待选策略在余量内取得私有写入，共享生产者沿实际引用结算。主路准备新增列表或屏幕资源时，已有离屏列表消费者优先结算驻留。额外准备失败时撤回本路写入、恢复预留并报告原因，有效策略仍可按当前相机与布局交付。已就绪的当前请求可以独立替代附件不兼容的旧策略。编译图按真实消费者保留生产者。Forward+ 与 Deferred 沿同一 View 的 Froxel 配方交付，切回不再消费列表的 Forward 时释放相应预留。 [上传所有权](../../../../Engine/Renderer/Materials/StaticGpu.cpp) · [双 View 准备](../../../../Runtime/Frame.cpp)

设备驻留按实际分配的字节和条目结算。四个 Froxel 缓冲共用一个记账对象，只有缺少兼容槽内存储的屏幕常量才增加设备条目。有效策略在规划中登记尚待分配的容量。首次或待选策略开始物化前，按自身完整分配及另一仍在准备的有效策略所需的余量一起准入。已分配的对象随实际寿命记账，取消的计划退出需求。这样两路同时切换时，冷屏幕候选可以等待，暖缓存候选仍可使用它的实际余量。 [驻留合同](../../../../Docs/Architecture.md#gpu-residency) · [路径资源准入](../../../../Engine/Renderer/Paths/Forward.cpp)

表面路由改变时沿当前有序绘制记录重新划分命令范围。原生布局、对象页和实例块匹配的 set 1 由新批次共同持有。屏幕常量的待写载荷由本轮上传批持有，完整图接受相应的存活写入后才更新缓存的字节身份。准备失败或写入未录制时保留原先接受的字节，已尝试写入但缺少接受回执时在安全槽再次使用前补写。 [批次及载荷准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [写入确认](../../../../Engine/Renderer/Paths/Forward.cpp)

### 沿正常提交确认路径和历史

准备状态按 View、Scene 和现有请求修订记录，候选使用本轮相机、内容、布局、显示设置与完整 Shader 代。较早请求形成的兼容缓存成果可服务于更新请求，其回执须经过当前身份复验。候选在构图、物化或执行中失败时沿既有图及票据协议收尾。保留同 View 的拒绝原因，旧色或其它 View 已可交付的结果沿现有恢复路径处理。共享正常提交失败与设备 Fatal 仍由宿主既有合同处理。 [待选身份与确认](../../../../Engine/Renderer/View.cpp) · [提交合同](../../../../Docs/Architecture.md#history-commit)

主路以本轮正常颜色的实际采样确认，附加路以自身的匹配导出确认。复用 `PreparePreview`、`Publish` 与 Runtime 的正常图回执，将真实路径、显示与渲染布局、实际 AA、生产阶段、配对 HDR/D32、相机、票据一次关联。显示色、层叠 UI 和导航映射对应实际接受的输出；按 View/Scene/票据读取的统计与延迟计时使用原提交的身份。安全 UI、裁剪和部分执行的资源保护票据保持既有语义。 [视口接受](specs/editor-viewport-output/spec.md#requirement-confirm-effective-viewport-settings-from-accepted-output) · [统计合同](../../../specs/render-frame-statistics/spec.md)

历史资格比较实际路径、栅格、投影/cut、Scene 内容、格式/样本和已用生产角色的接口及内容。连续位姿和曝光沿兼容生产合同推进；路径和 resize 在各自改变的成功帧结算。拒绝、取消和只读旧图保持相应记录，另一 View 按自身回执计算。兼容管线及驻留继续复用，旧输出和旧帧引用按 GraphRetainedResource 和 FrameSlot 的实际票据/完成状态保活。切换调用链使用局部准备与退役，无场景重载、RenderEngine 重建或全设备等待。 [历史合同](../../../../Docs/Architecture.md#temporal-contract) · [资源池](../../../specs/render-graph-resource-pool/spec.md) · [帧结束参考](../../../../Docs/Reference/Filament/FrameEndAndDraw.md#f14)

### 扩展现有字段意图及文件值域

在 `OpaquePath` 的类型和值域边界增加 Deferred，保留现有 Forward 和 Forward+ 值。View Settings 使用对应选项，经 `OpaquePathIntent` 提交匹配当前 Scene/View 的字段编辑。重复请求、Undo/Redo、文档冻结及被替换文档沿既有事务处理。面板读取当前请求、已确认策略和准备原因，按实际接受记录显示 AA 与布局。离屏 View 使用类型化请求，其状态和生命周期归调用方。 [领域编辑](specs/editor-domain-edits/spec.md#requirement-edit-and-inspect-the-main-view-opaque-strategy) · [现有面板](../../../../Editor/Panels/Workspace.cpp)

## Risks / Trade-offs

- 待选计划借用前一路的路由或只准备颜色键时，可能出现路径已选而 GBuffer、辅助或绑定不匹配。以受光核心、MASK、Unlit 的正常图身份和小范围原生 HDR/D32/显示读回核对。
- 启动 Deferred 会增加当次表面用途及可能的 Froxel 准备。一旦候选退出，检查其专属写入和附件已撤销，另一 View 的当前生产者仍可读。用小额 native 预算和角色拒绝区分等待、失败与恢复。
- 先后相同的请求值、resize 或旧图呈现可能误确认过期候选。用身份与真实正常回执的时序对照核对接受、历史资格和跨 View 隔离。

## Migration Plan

先接入类型和值域及固定策略组装，再让主 View 和显式离屏入口共同消费准备和接受合同，最后核对 UI、文档与回执。v1 继续用省略字段表示 Forward，`forward-plus` 保留，`deferred` 写明新请求。存档只捕获主 View 已接受编辑的意图，打开时按完整文档发布并重新准入。旧客户端读含新值的场景时会报原字段错误；要在该客户端继续使用可交换的场景，可在当前客户端选择旧值后另存。 [v1 文档规格](specs/scene-document-persistence/spec.md#requirement-persist-only-the-main-views-supported-requests)
