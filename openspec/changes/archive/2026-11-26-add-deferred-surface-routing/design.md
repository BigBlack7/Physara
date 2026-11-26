# Design

## Context

D11-T01 的 `DeclareCoreSurface` 已为显式请求生成四附件和 D32。当前 `StaticDraw` 仅按 `unlit` 拆分核心批次，`RasterInputs` 跳过 Unlit；普通工作色和主深度仍由 Forward 单独产生。现有前置深度颜色 Pipeline 已支持只读 D32 和 Equal；图资源支持附件 Load、整数清除及 R16_UINT 的无符号 fetch。 [核心生产与图入口](../../../../Engine/Renderer/Paths/Forward.cpp#L1539) · [批次准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp#L1211) · [原生光栅状态](../../../../Engine/Renderer/Materials/StaticGpu.cpp#L756)

## Goals / Non-Goals

把表面准备和辅助颜色做成 Renderer 私有的图组装入口。两处共同消费冻结 Draw、图版本、有效渲染矩形和就绪 Pipeline。T03 可从这两个入口之间插入核心光照。正常 View 路径选择和接受事务由 T04 接入。 [提案](proposal.md) · [表面规格](specs/core-gbuffer/spec.md)

## Decisions

### CPU 在批次准备时确定路由

从现有有效材质及其驻留版本派生 `CoreDeferred`、`Unlit`、`OpaqueForwardExtension`。以材质 profile 和支持状态准入。当前 glTF 的纹理变换与曝光前自发光强度参数仍按核心字段求值。当前源资产可直接提供 Unlit，有效覆盖会更新 alpha 模式、阈值、因子和纹理。后续 D12 已验证的非核心 lit profile 可使用扩展路由。透明拒绝沿用现有入口。 [有效材质](../../../../Engine/Scene/SceneMaterial.hpp#L16) · [当前 glTF 支持](../../../../Engine/Asset/GltfScene.cpp#L13)

在原有完整排序之后仅按路由边界拆相邻批次。核心 Pass 筛出核心批次，SurfaceAux 与其颜色重画读取同一辅助批次序列，并沿用该序列的实例行、绑定、间接参数和顶点位置。MASK 继续以有效材质的覆盖参数处理，路由与覆盖独立。从材质名决定 shader 或在 Pass 中重新扫描材质会让前后两次绘制有机会采用不同路由。

### SurfaceAux 接续同一物理表面

`PrepareDeferredSurface` 先声明核心生产并明确定义背景，随后在有辅助批次时按 Load 接续写 G1、G3 与 D32。D32 使用 GreaterOrEqual。核心之后的辅助片元在等深时胜出，辅助几何内部以原顺序的最后覆盖为准。核心区和留边保持先前已经定义的值。纯 Unlit 图仍由核心阶段建立定义好的清除值，再由 SurfaceAux 产生覆盖。空辅助集合直接返回核心图版本。

交付同一矩形下最终 D32、G1、G3 以及匹配的核心附件身份。G0、G2 沿用核心生产时的版本，其数据仅在核心资格有效的像素解释。G1 写八面体世界着色法线、一次过滤后的感知粗糙度，Aux 的 W 写零。共用材质梯度、MASK、切线框架与双面处理，普通 Unlit 的基色合同独立于所交付的辅助法线和粗糙度。G3 分类由生成接口提供整值常量，完整覆盖时写入对应分类。现有位 0 判定仍适用，远平面与背景由 G3 区分。

### 用同一生成接口描述紧凑的两个颜色附件

Graph 和 RHI 以连续数组声明颜色附件。保持 `CoreGBufferOutput` 按逻辑 G0–G3 顺序交付，原生角色共享以下位置映射。用一份 Renderer 配方驱动图附件顺序与 Pipeline FramebufferInfo，由生成接口及包反射验证数值型。

| 片元输出位置 | Core GBuffer | SurfaceAux |
|---:|---|---|
| 0 | G0 · RGBA8_SRGB | G1 · RGBA16_FLOAT |
| 1 | G3 · R16_UINT | G3 · R16_UINT |
| 2 | G2 · R11G11B10_FLOAT | — |
| 3 | G1 · RGBA16_FLOAT | — |

在 `StaticForward` 生成接口交换整数 flags 与核心法线材质输出的位置。SurfaceAux 使用共享的 vec4 位置 0 与 uint 位置 1。逻辑 GBuffer 字段、格式、单样本和存储用量保留 T01 合同。此原生位置调整带来当前 StaticForward 包 ABI 换代。沿用四个附件同时声明会给仅做辅助写入的阶段增加 G0/G2 的 Load 和覆盖依赖。使用已有紧凑附件能力把变更限定在角色配方和当前生成接口。 [附件声明](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp#L242) · [包角色核对](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp#L61)

SurfaceAux 通过产品包目录和同一 Shader 代准备。片元只消费实际所需的有效材质行、覆盖、粗糙度和法线资源。顶点保留实例位置及法线 UV 框架所需输入。按角色核对活动资源和输出，复用现有组装前准备、冷包准入及热重载完整发布。普通 View 仅为自己的活跃 Pass 预备原生对象。错误角色或未就绪角色沿现有准入结果返回，保留旧有效代及其它可准备 View。 [准备与热重载合同](../../../../Docs/Architecture.md#plan-readiness)

### 在最终只读深度上合成辅助颜色

`AddOpaqueForwardExtensions` 接受同 View 的上游工作色、最终表面和冻结准备数据，按 Load 接续颜色版本。深度只声明 `DepthStencilRead`，使用现有 Equal/关闭深度写入的前向状态。辅助合成入口保留共享顶点位置和覆盖求值、同一辅助批次次序及着色模型。Unlit 输出线性基色，受支持的扩展依其前向材质输出工作色，存活片元把覆盖 alpha 写为 1。上游核心颜色消费者按最后 G3 的位 0 决定是否贡献。用 G3 选择核心表面和用等深重画选择辅助表面是两个相互配合的判定，避免颜色覆盖掉不同深度的核心表面。后继天空直接沿用工作色覆盖 alpha。

显式表面需求可单独交付最终 GBuffer，或通过 Renderer 内部的 `ViewResourceNeeds::shadeDeferredCore` 声明核心颜色生产者。后一组合把最终表面 D32 作为该路主深度，前向 Pipeline 需求限于辅助批次。原生验收使用同图的开发期核心标记 Pass，按最后 G3 通过共享资格判定写工作色，并保留一份合成前颜色进行读回。标记 Pass 放在 `build/d11-t02/` 的验证材料中。T03 接续以生产 DeferredLighting 交付核心工作色。 [共享路径组装](../../../../Docs/Architecture.md#paths) · [Forward 颜色和覆盖](../../../../Shaders/Passes/Forward/Color.glsl)

前向受光输入取决于仍会执行前向颜色的有效材质。普通前向和辅助重画按所选颜色角色绑定实际灯表或共享零表。纯核心组合保留栅格使用的 View 常量。所选颜色角色统一决定对应 Froxel 的预留、绑定和上传结算。最终 HDR 与深度的生产身份记录已使用的核心、辅助及前向程序与共同阶段。核心回调尚无稳定生产身份时，该次输出仍可接受，历史从本帧重新开始。有效阴影诊断与核心颜色合成在规划入口返回明确的不兼容原因。独立 GBuffer 请求继续沿用普通诊断帧的颜色和主深度。 [历史资格](../../../../Docs/Architecture.md#temporal-contract)

## Risks / Trade-offs

- **路由或覆盖顺序漂移。** 同一对重叠实例提供孔、阈值和更远辅助几何的独立预期。另以共面异批标样核对 G1、G3 和最终颜色由同一 Draw 决定。T02 的跨路由等深规则适用于新的 Deferred 表面组装，普通 Forward 的已有次序仍由其自身合同定义。
- **G3 数值型失配或附件版本残留。** 以原生混合附件读回验证新角色和核心的逻辑附件映射，测试远平面与实际池复用后的清除背景。用一个与角色不符的包核对冷准备和热重载均定位错误。
- **共享接口换代影响既有路径。** 以同一生成输入重建受影响的产品包，并在匹配版本运行既有核心字段和普通路径的代表性对照。包结构校验与实机像素证据各自记录。

## Migration Plan

按生成接口与产品角色、Draw 路由、表面及颜色组装的顺序接入。随产品构建统一生成当前接口的包，部署时同批更新程序及其包。回滚以整套上一构建及其包为单位。场景 v1 通过原有读写入口。T02 验收后，T03 从最后的 GBuffer/深度消费表面并向同一颜色入口接续。
