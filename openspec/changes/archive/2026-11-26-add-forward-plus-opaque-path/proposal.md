# Proposal

## Why

D10-T02 已交付可由图消费的 Froxel 列表、拥有型上传和完整灯退路。D10-T03 将这些输入接入正常静态场景渲染，使 View 能准备并实际执行 Forward+ 不透明策略，供后续交互切换使用。 [D10-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t03) · [D10-T02 验收](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification)

## What Changes

- 从正常帧的 View 请求选择 Forward 或 Forward+。初始路径为 Forward。Forward+ 的 Direct Lighting 和 Physical Lighting 共享材质、表面、阴影、环境光和自发光求值，仅在局部灯候选上使用 Froxel。原有材质预览和阴影诊断沿用各自的显示合同。
- 为实际使用 Froxel 的不透明颜色角色准备有限的产品 Shader 变体。打包和热重载均按角色核对 PerView 资源及绑定配方。用同轮 View 的图写版本向颜色 Pass 提供网格参数、header、有限索引和无范围局部灯索引。
- 通过现有帧前准备和结果确认接入 Ready/Pending/Failed。已就绪方案可在帧边界用于单样本 None/FXAA 正常图，保留当前场景、几何与材质驻留以及编辑状态。准备或提交未成功时保留原因及上次可用输出。
- D10-T04 交付编辑器的路径选择、持续执行当前有效计划的切换事务及兼容性驱动的历史失效。D10-T05 负责同画质 HDR 差分与 CPU/GPU 成本的阶段验收。 [D10-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t04) · [D10-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05)

## Capabilities

### New Capabilities

沿用现有能力。

### Modified Capabilities

- `static-forward-rendering` 增加同一不透明子图内的 Forward+ 候选消费及准确的图依赖。 [增量规格](specs/static-forward-rendering/spec.md)
- `view-preparation` 增加每 View 的不透明策略请求、计划就绪条件与帧边界确认。 [增量规格](specs/view-preparation/spec.md)
- `shader-generation-reload` 增加全灯及 Froxel 产品角色的活动资源准入合同。 [增量规格](specs/shader-generation-reload/spec.md)

## Impact

- `Renderer/Paths`、`Renderer/Draw` 和 `Shaders/Passes/Forward` 共享表面准备、几何绘制及 `OpaqueOutput` 的语义，沿用既有天空与显示消费者。 [子图合同](../../../../Docs/Architecture.md#paths) · [不透明光照](../../../../Docs/Architecture.md#opaque-shading)
- `Renderer/View`、`Renderer/Prepare` 和 `Runtime` 在现有正常帧入口解析及确认每 View 的方案。`Engine/Shader` 和现有产品包构建、加载及热重载扩展有限角色目录与绑定校验。 [计划准备](../../../../Docs/Architecture.md#plan-readiness) · [Shader 构建](../../../../Docs/Architecture.md#shader-build)
- 继承已采纳的 32 位 Froxel 数据、overflow 和无有效 cell 退路、图上传及统计口径。 [Froxel 准备](../../../specs/froxel-light-preparation/spec.md) · [帧统计](../../../specs/render-frame-statistics/spec.md)
