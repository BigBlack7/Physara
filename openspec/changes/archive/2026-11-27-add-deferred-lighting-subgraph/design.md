# Design

## Context

`PrepareDeferredSurface` 已写出 Core GBuffer、可选 SurfaceAux 和最终 D32。`DeferredCorePass` 回调只能得到这些图版本及简要 View 来源，现有资源规划也只为仍绘制前向颜色的材质准备灯与 Froxel。生产子图需要在同一冻结帧内取得这些输入并把核心颜色纳入准备、执行和接受记录。 [提案](proposal.md) · [现有路径](../../../../Engine/Renderer/Paths/Forward.cpp) · [准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [D11-T02 验收](../2026-11-26-add-deferred-surface-routing/tasks.md#final-verification)

`Color.glsl` 已调用共享全灯及 Froxel 入口，但聚合仍从原始自发光开始单独应用曝光，并由颜色 Pass 追加 IBL。G2 交付的是已曝光工作色。现有运行时支持 Forward 和 Forward+ 请求。显式表面需求和已有的 Froxel 准备入口足以接入本次内部子图。 [颜色入口](../../../../Shaders/Passes/Forward/Color.glsl) · [编解码](../../../../Shaders/Material/CoreGBuffer.glsl) · [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.hpp)

## Goals / Non-Goals

**Goals**

- Renderer 对内部 Deferred 请求提供可准备、可构图和可提交的单样本受光计划，保持统一的表面、光照与不透明输出合同。Shader、几何和图资源各由现有所有者维护。
- 复用相同的逐灯、IBL、曝光、辅助合成、天空和显示消费者。按真实读项建立资源准备及图上的依赖。

**Non-Goals**

- View 和 Editor 的策略注册、None/FXAA 配置接受及事务切换由 D11-T04 完成。此处的显示来自已经交付的模式与显示组件。三路径全域差分、预览和成本验收归 D11-T05。
- D12 扩展更多不透明材质，D13 加入透射及透明生产，D14 接入屏幕效果，D15 接入 TAA。当前实现只消费已支持的静态受光数据和明确的屏幕效果默认输入。 [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04) · [阶段索引](../../../../Docs/Developments/Developments.md)

## Decisions

### 用 Renderer 内部的显式请求对接现有准备链

将生产 Deferred 由内部的类型化需求交给 `ForwardRenderer` 的 `PlanView`、帧前 Shader 准备、`FinishView` 和 `ForwardFrame::Declare`。用正式核心着色入口接续 T02 的回调位置。`coreGBuffer` 继续给普通颜色附加一份最终表面，`surfaceOnly` 仅请求最终 GBuffer 与配对 D32。有效阴影诊断与 Deferred 受光的组合，以及仅表面与另选颜色策略的组合，在准备写入前由现有 `InvalidViewRequest` 合同拒绝。产品入口面向当前 Direct Lighting、Physical Lighting 和同一单样本 D32。可编辑的 `OpaquePath` 注册、requested/effective 发布和策略回退的整合集中在 T04。 [准备入口](../../../../Engine/Renderer/Paths/Forward.hpp) · [路径组装](../../../../Docs/Architecture.md#paths)

由冻结的已准入批次识别核心颜色及辅助颜色的实际需要。`PrepareSurface` 产出带最终 D32、G0–G3 与渲染矩形的 `SurfaceOutput`，其中最终 G1 和 D32 表达最近的核心或辅助表面。随后准备同 View 的 `LightingInputs`，屏幕输入在现阶段是单位遮蔽以及零置信度、零预乘反射辐亮度的值，沿用材质 AO。`ShadeDeferredOpaque` 建立核心工作色，再接续现有辅助颜色。共享天空和显示消费同一 `OpaqueOutput` 的 HDR、D32 及可供当前消费者使用的表面信息，继续从既有 `ForwardOutput` 交付。现有入口外的算法副本会另外承担图与工作域合同，故在 `DeclareView` 的共享收尾中接续。 [路径组装](../../../../Docs/Architecture.md#paths) · [表面合同](../../../../Docs/Architecture.md#geometry) · [Filament 前置表面数据](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f9)

### 按当前矩形和逆渲染投影恢复几何

从冻结 `CameraSnapshot` 取得 `viewFromRenderClip`、`worldFromView` 的旋转基及 FP64 眼点，打包同一 View 的生成常量。将眼点编码为 FP32 高位、低位和低位舍入余量。主光栅和屏幕恢复读取同一 `ForwardView` 中的三段，Froxel 以同一 CPU 编码构造其稳定查询数据。片元按 `DeferredEligible` 门控其余 GBuffer 采样。由有效矩形里的像素中心计算 UV，再通过 `(2u − 1, 1 − 2v, depth, 1)` 和齐次除法恢复 view-space 位置。共享坐标约定涵盖 `[0,1]` Reverse-Z 和左上图像原点。有限正交远面允许 D32=0。逆矩阵携带本次透视或正交的远面语义与实际抖动。 [相机快照](../../../../Engine/Renderer/Prepare/CameraSnapshot.hpp) · [坐标合同](../../../../Docs/Architecture.md#conventions)

主光栅先用对象行的高低位偏移和同次眼点作差，将两侧编码余量及有符号运算余差合回相对位移，再使用相机旋转与当前渲染投影产生裁剪位置。核心表面、辅助表面、等深重画及普通前向使用同一顶点入口。阴影写入和接收查询共享子视图的三段投影原点及补偿作差，级联选级使用主 View 的眼点。写入沿用常规深度矩阵和 caster 偏移。沿视深共同平移的对照覆盖眼点低位为零、非零且可精确编码，以及低位窄化还有余量三个样例。 [光栅相机编码](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [静态顶点](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [相对作差](../../../../Shaders/Geometry/LightingPosition.glsl)

保留相机相对偏移，旋转后以眼点高位为世界 anchor，将重建偏移、眼点低位与编码余量分开交给 `LightingSurface`。透视 V 由相对量朝向眼点，正交 V 由相机基提供。稳定格查询沿用同一受光位置的稳定投影和正向视深，保守界纳入 D32/逆变换误差及本次相对作差的未合入项。仅在误差范围可归于一个有效格时采用该格，沿共享无效格哨兵进入完整灯表退路。正向视深及边界对照以冻结投影和独立几何交点为准。 [位置与余差](../../../../Shaders/Geometry/LightingPosition.glsl) · [Froxel 求值](../../../../Shaders/Lighting/FroxelLighting.glsl) · [灯光合同](../../../../Docs/Architecture.md#lighting)

### 从深度提供阴影接收几何，从 GBuffer 提供着色法线

G1 的单位法线已应用法线贴图和面向，只进入 BRDF。带阴影的核心颜色角色从有效矩形内的核心深度邻点估计局部几何法线。每轴优先采用在 D32 精度下符合中心及该侧连续深度的短差分，邻点取样受有效矩形和核心资格约束。用恢复后的相对位置构造切向和归一化法线。缺少可用邻点、出现深度断层或差分退化时，使用零法线偏移并继续在原接收位置做共同的 Hard/PCF 比较。正反面和法线朝光侧处理遵循现有查询。取样估计限于可见局部几何，平滑插值法线与深度平面在轮廓处的差异计入阴影近似。将 G1 直接用于偏移会使偏移随贴图转动。通过深度估计可继续使用固定 18 B/像素的颜色布局。 [接收偏移](specs/shadow-rendering/spec.md) · [共享查询](../../../../Shaders/Shadow/Visibility.glsl) · [几何合同](../../../../Docs/Architecture.md#geometry)

### 共用受光运算及匹配的资源配方

把现有颜色入口之间共用的 `PixelParams`、观察项、DFG、直接光及 IBL 聚合放在现有 Shader 库。几何阶段从材质与覆盖求值，Deferred 从同屏 G0、G1、G2 解码，解码复用核心参数构造。共享聚合接收工作域自发光，几何片元在入口使用相同工作域缩放函数，Deferred 直接传入 G2。保持现有 emission、方向灯、按灯行号归并的局部灯及 IBL 累加和限幅顺序。从几何导数得出的粗糙度只在原几何片元过滤一次，解码后按该值重新求 view-dependent DFG。使用不依赖几何 varying 的共用类型和库函数，材质取样仍由几何入口负责。 [共享光照](../../../../Docs/Architecture.md#lighting) · [材质与 DFG](../../../../Docs/Architecture.md#materials) · [Filament 光照调用链](../../../../Docs/Reference/Filament/Lighting.md#f11)

在原有 `StaticForward` 生成接口中表达片元专用的屏幕常量和只读 GBuffer 配方，维持唯一的共同灯表和 View 数据 ABI 定义。G3 绑定为无符号 2D 采样资源，D32 为深度 aspect 的单样本读取。用全屏三角形建立实际角色，直接光、带 DFG、带 IBL 以及各自的阴影版本沿用现有有限结构组合，不透明 HDR 目标固定。CPU 角色校验以阶段、类型、绑定、现有实际灯资源及范围为准。Deferred 活动项包括屏幕读项和该变体所需灯光资源，几何配方使用自身的活动资源。级联、聚光和点光诊断共用运行时阴影组分派，诊断片元的活动配方包含 set 0 的 `perView`，供级联分支取得主眼点。光栅顶点的 `perView` 按所属阶段独立核对。产品目录、默认生成和运行期资源定位使用同配置产物。冷包与热重载共用角色准入与原生准备预算，冻结帧持有实际使用的完整 Shader 代。`OpaqueProduction` 记录核心屏幕角色的包身份，并沿用本次确实使用的辅助、阴影与天空身份。 [生成接口](../../../../Engine/Shader/Interface/StaticForward.json) · [候选准备](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [角色准入合同](../../../specs/shader-generation-reload/spec.md#requirement-admit-active-resources-according-to-the-opaque-shader-role)

### 从消费需求驱动图生产和交付

直接沿用每 View 的灯表、Froxel 四块上传、阴影引用和环境缓存。生产核心颜色和辅助 lit 都消费同 View 的匹配输入。两者并存时共用上传及阴影版本。显式 Deferred 受光可以从既有 ViewWork 显式 Froxel 请求取得同一冻结候选，准备和收尾以实际颜色消费者驱动预算、绑定及撤回。CPU 列表缺失按已有准备失败原因处理。仅 Unlit 或背景的颜色候选跳过核心受光，辅助 Unlit 的传统前向配方可按绑定需求持有现有零表，天空按实际环境请求准备。仅表面候选在规划、收尾和构图中只需表面所用的相机、几何、材质、角色及附件。颜色与照明包、灯表、Froxel、阴影、天空及显示资源均不参与该候选。 [Froxel 资源合同](../../../specs/froxel-light-preparation/spec.md) · [环境资源合同](../../../specs/environment-lighting/spec.md)

| 阶段 | 声明的主要图访问与输出 |
|---|---|
| Core GBuffer 与 SurfaceAux | Core 初始化 G0–G3/D32。Aux 以 Load 更新 G1/G3/D32。G0、G2 保持 Core 的逻辑版本。 |
| DeferredLighting | 整个 RGBA16F 目标明确清为现有背景 RGB、alpha=0，有效矩形内按 G3 资格写 alpha=1。采样四附件和最终 D32 的 Depth aspect，读取当轮 View、灯块和实际环境/阴影范围。无核心色需求仍有完整初始化来源。 |
| 辅助颜色 | Load 工作色，读取最终 D32 的 `DepthStencilRead`，现有 Equal 测试、关闭深度写，保持 T02 的辅助顺序。 |
| Sky 与显示 | Sky Load 工作色，以目的覆盖 alpha 合成，Display 读取最新 HDR。深度输出始终为表面准备的末版。 |

同一 D32 的深度采样走现有 `ShaderResource | DepthRead`，只读深度附件沿图的用途合并与 backend 能力处理。产品屏幕颜色 Pass 只采样 D32，辅助颜色 Pass 使用只读深度附件，各自声明实际用途。当同一 Pass 真正同时采样并做只读深度测试时，现有 GL 联合只读能力负责合法性及写后屏障。以同一深度生产链上的最小联合读取验证该合同，并核对样点和深度版本。 [图编译](../../../../Docs/Architecture.md#graph-compile) · [命令和状态](../../../../Docs/Architecture.md#command-execution) · [联合只读规格](../../../specs/render-graph-compilation/spec.md)

## Risks / Trade-offs

- D32 和矩阵精度会影响近边界的着色位置。用非对称、带留边的投影平面、有限远面和可区分局部灯检查坐标及通道结果，光照候选不确定时沿用全灯退路。
- 屏幕接收几何只能估计可见平面，轮廓和细几何可能缺有效法线。用带法线贴图的连续斜面及有效邻点不足的区域验证接收偏移和实际可见性。把代表性差异交给 T05 的整场等价口径。
- 核心照明成为灯表及 Froxel 的新读者，跨 View 借错版本或活动角色遗漏都可能产生合法提交的错误颜色。在同图核对准备配额、写与读身份、原生绑定和输出，并以缺失活动项的产品角色验证拒绝。
- 新旧颜色入口共用 Shader 接口和聚合，接口变更会重新生成该族的同配置包。已有 Forward/Forward+ 签收场景给出可独立复现的颜色与覆盖基线。

## Migration Plan

先补齐生成 ABI、同配置产品角色与共享 Shader 聚合，再接内部计划准备和生产图，最后沿真实消费者回放验收。开发期 T02 的标记回放仍可从独立 GBuffer 取得资格及深度，实际颜色验收以产品 DeferredLighting 读取为准。兼容输出退路继续由现有 View 与 Shader 代持有。后续 T04 直接使用这条已就绪的内部接线。 [实现及验收任务](tasks.md) · [A/B 交接](../../../../AGENTS.md#review-loop)
